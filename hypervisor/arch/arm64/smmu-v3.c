/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Authors:
 *  Lokesh Vutla <lokeshvutla@ti.com>
 *  Pratyush Yadav <p-yadav1@ti.com>
 *
 * The guest cells are assigned stream IDs in their configs and only those
 * assigned stream IDs can be used by the cells. There is no checking in place
 * to make sure two cells do not use the same stream IDs. This must be taken
 * care of when creating the cell configs.
 *
 * This driver is implemented based on the following assumptions:
 * - Running on a Little endian 64 bit core compatible with ARM v8 architecture.
 * - SMMU supporting only AARCH64 mode.
 * - SMMU AARCH 64 stage 2 translation configurations are compatible with ARMv8
 *   VMSA. So re using the translation tables of CPU for SMMU.
 *
 * This driver is loosely based on the Linux kernel SMMU v3 driver.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See the
 * COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <asm/control.h>
#include <jailhouse/unit.h>
#include <asm/iommu.h>
#include <jailhouse/cell.h>
#include <jailhouse/mmio.h>

/* Offset of addr from start of the page. */
#define PAGE_OFFSET(addr)		((addr) & PAGE_OFFS_MASK)

#define LOWER_32_BITS(n)		((u32)(n))
#define UPPER_32_BITS(n)		((n) >> 32)

/* MMIO registers */
#define ARM_SMMU_IDR0			0x0
#define IDR0_ST_LVL			BIT_MASK(28, 27)
#define IDR0_TTENDIAN			BIT_MASK(22, 21)
#define IDR0_VATOS			(1 << 20)
#define IDR0_VMID16			(1 << 18)
#define IDR0_PRI			(1 << 16)
#define IDR0_ATOS			(1 << 15)
#define IDR0_MSI			(1 << 13)
#define IDR0_ASID16			(1 << 12)
#define IDR0_NS1ATS			(1 << 11)
#define IDR0_ATS			(1 << 10)
#define IDR0_S2P			(1 << 0)
#define IDR0_S1P			(1 << 1)
#define IDR0_HTTU			BIT_MASK(7, 6)
#define IDR0_COHACC			(1 << 4)
#define IDR0_TTF			BIT_MASK(3, 2)

#define IDR0_TTF_AARCH64		2
#define IDR0_TTENDIAN_LE		2
#define IDR0_ST_LVL_2LVL		1

#define ARM_SMMU_VMID8_MAX_VMID		255

#define ARM_SMMU_IDR1			0x4
#define IDR1_TABLES_PRESET		(1 << 30)
#define IDR1_QUEUES_PRESET		(1 << 29)
#define IDR1_REL			(1 << 28)
#define IDR1_CMDQS			BIT_MASK(25, 21)
#define IDR1_EVTQS			BIT_MASK(20, 16)
#define IDR1_SSIDSIZE			BIT_MASK(10, 6)
#define IDR1_SIDSIZE			BIT_MASK(5, 0)

#define ARM_SMMU_IDR2			0x8
#define ARM_SMMU_IDR3			0xC
#define ARM_SMMU_IDR4			0x10
#define ARM_SMMU_IDR5			0x14

#define ARM_SMMU_CR0			0x20
#define CR0_CMDQEN			(1 << 3)
#define CR0_EVTQEN			(1 << 2)
#define CR0_SMMUEN			(1 << 0)

#define ARM_SMMU_CR0ACK			0x24

#define ARM_SMMU_CR1			0x28
#define CR1_TABLE_SH			BIT_MASK(11, 10)
#define CR1_TABLE_OC			BIT_MASK(9, 8)
#define CR1_TABLE_IC			BIT_MASK(7, 6)
#define CR1_QUEUE_SH			BIT_MASK(5, 4)
#define CR1_QUEUE_OC			BIT_MASK(3, 2)
#define CR1_QUEUE_IC			BIT_MASK(1, 0)
/* CR1 cacheability fields don't quite follow the usual TCR-style encoding */
#define CR1_CACHE_NC			0
#define CR1_CACHE_WB			1
#define CR1_CACHE_WT			2

#define ARM_SMMU_CR2			0x2c
#define CR2_PTM				(1 << 2)
#define CR2_RECINVSID			(1 << 1)
#define CR2_E2H				(1 << 0)

#define ARM_SMMU_STRTAB_BASE		0x80
#define STRTAB_BASE_RA			(1UL << 62)
#define STRTAB_BASE_ADDR_MASK		BIT_MASK(51, 6)

#define ARM_SMMU_STRTAB_BASE_CFG	0x88
#define STRTAB_BASE_CFG_FMT		BIT_MASK(17, 16)
#define STRTAB_BASE_CFG_FMT_LINEAR	0
#define STRTAB_BASE_CFG_FMT_2LVL	1
#define STRTAB_BASE_CFG_SPLIT		BIT_MASK(10, 6)
#define STRTAB_BASE_CFG_LOG2SIZE	BIT_MASK(5, 0)

#define ARM_SMMU_CMDQ_BASE		0x90
#define ARM_SMMU_CMDQ_PROD		0x98
#define ARM_SMMU_CMDQ_CONS		0x9c

#define ARM_SMMU_EVTQ_BASE		0xa0
#define ARM_SMMU_EVTQ_PROD		0x100a8
#define ARM_SMMU_EVTQ_CONS		0x100ac
#define ARM_SMMU_EVTQ_IRQ_CFG0		0xb0
#define ARM_SMMU_EVTQ_IRQ_CFG1		0xb8
#define ARM_SMMU_EVTQ_IRQ_CFG2		0xbc

#define ARM_SMMU_GERROR			0x60
#define GERROR_CMDQ_ERR			(1 << 0)
#define GERROR_EVTQ_ABT_ERR		(1 << 2)

#define ARM_SMMU_GERRORN		0x64
#define ARM_SMMU_IRQ_CTRL		0x50
#define ARM_SMMU_IRQ_CTRLACK		0x54
#define ARM_SMMU_GERROR_IRQ_CFG0	0x68
#define ARM_SMMU_EVTQ_IRQ_CFG0		0xb0

/* Common memory attribute values */
#define ARM_SMMU_SH_NSH			0
#define ARM_SMMU_SH_OSH			2
#define ARM_SMMU_SH_ISH			3
#define ARM_SMMU_MEMATTR_DEVICE_nGnRE	0x1
#define ARM_SMMU_MEMATTR_OIWB		0xf

#define Q_IDX(reg, shift)		((reg) & ((1 << (shift)) - 1))
#define Q_WRP(reg, shift)		((reg) & (1 << (shift)))
#define Q_OVERFLOW_FLAG			(1 << 31)
#define Q_OVF(reg)			((reg) & Q_OVERFLOW_FLAG)
#define Q_EMPTY(prod, cons, shift)	\
			(Q_IDX((prod), (shift)) == Q_IDX((cons), (shift)) && \
			 Q_WRP((prod), (shift)) == Q_WRP((cons), (shift)))
#define Q_FULL(prod, cons, shift)	\
			(Q_IDX((prod), (shift)) == Q_IDX((cons), (shift)) && \
			 Q_WRP((prod), (shift)) != Q_WRP((cons), (shift)))

#define Q_BASE_RWA			(1UL << 62)
#define Q_BASE_ADDR_MASK		BIT_MASK(51, 5)
#define Q_BASE_LOG2SIZE			BIT_MASK(4, 0)

/*
 * Stream table.
 *
 * Linear: Enough to cover 1 << IDR1.SIDSIZE entries
 * 2lvl: 128k L1 entries,
 *       256 lazy entries per table (each table covers a PCI bus)
 */
#define STRTAB_L1_SZ_SHIFT		20
#define STRTAB_SPLIT			8

#define STRTAB_L1_DESC_DWORDS		1
#define STRTAB_L1_DESC_SIZE		(STRTAB_L1_DESC_DWORDS << 3)
#define STRTAB_L1_DESC_SPAN		BIT_MASK(4, 0)
#define STRTAB_L1_DESC_L2PTR_MASK	BIT_MASK(51, 6)

#define STRTAB_STE_DWORDS_BITS		3
#define STRTAB_STE_DWORDS		(1 << STRTAB_STE_DWORDS_BITS)
#define STRTAB_STE_SIZE			(STRTAB_STE_DWORDS << 3)
#define STRTAB_STE_0_V			(1UL << 0)
#define STRTAB_STE_0_CFG		BIT_MASK(3, 1)
#define STRTAB_STE_0_CFG_ABORT		0
#define STRTAB_STE_0_CFG_BYPASS		4
#define STRTAB_STE_0_CFG_S1_TRANS	5
#define STRTAB_STE_0_CFG_S2_TRANS	6
#define STRTAB_STE_0_S1CTXPTR		BIT_MASK(51, 6)
#define STRTAB_STE_0_S1CDMAX		BIT_MASK(63, 59)
#define STRTAB_STE_1_S1DSS		BIT_MASK(1, 0)
#define STRTAB_STE_1_S1CIR		BIT_MASK(3, 2)
#define STRTAB_STE_1_S1COR		BIT_MASK(5, 4)
#define STRTAB_STE_1_S1CSH		BIT_MASK(7, 6)
#define STRTAB_STE_1_S1STALLD		(1UL << 27)
#define STRTAB_CTXDESC_DWORDS		8
#define STRTAB_CTXDESC_S1CTXPTR_SHIFT	6

#define STRTAB_STE_1_SHCFG		BIT_MASK(45, 44)
#define STRTAB_STE_1_SHCFG_INCOMING	1UL

#define STRTAB_STE_2_S2VMID		BIT_MASK(15, 0)
#define STRTAB_STE_2_VTCR		BIT_MASK(50, 32)
#define STRTAB_STE_2_S2AA64		(1UL << 51)
#define STRTAB_STE_2_S2ENDI		(1UL << 52)
#define STRTAB_STE_2_S2PTW		(1UL << 54)
#define STRTAB_STE_2_S2R		(1UL << 58)

#define STRTAB_STE_3_S2TTB_MASK		BIT_MASK(51, 4)

#define CTXDESC_1_TTB0			BIT_MASK(51, 4)
#define CTXDESC_2_TTB1			BIT_MASK(51, 4)
#define CTXDESC_TTB0_SHIFT		4
#define CTXDESC_TTB1_SHIFT		4

/* Command queue */
#define CMDQ_ENT_DWORDS			2
#define CMDQ_ENT_SIZE			(CMDQ_ENT_DWORDS << 3)
#define CMDQ_MAX_SZ_SHIFT		8

#define CMDQ_CONS_ERR			BIT_MASK(30, 24)
#define CMDQ_ERR_CERROR_NONE_IDX	0
#define CMDQ_ERR_CERROR_ILL_IDX		1
#define CMDQ_ERR_CERROR_ABT_IDX		2

#define CMDQ_0_OP			BIT_MASK(7, 0)
#define CMDQ_0_SSV			(1UL << 11)

#define CMDQ_PREFETCH_0_SSID		BIT_MASK(31, 12)
#define CMDQ_PREFETCH_0_SID		BIT_MASK(63, 32)
#define CMDQ_PREFETCH_1_SIZE		BIT_MASK(4, 0)
#define CMDQ_PREFETCH_1_ADDR_MASK	BIT_MASK(63, 12)

#define CMDQ_CFGI_0_SID			BIT_MASK(63, 32)
#define CMDQ_CFGI_1_LEAF		(1UL << 0)
#define CMDQ_CFGI_1_RANGE		BIT_MASK(4, 0)

#define CMDQ_TLBI_0_VMID		BIT_MASK(47, 32)
#define CMDQ_TLBI_0_ASID		BIT_MASK(63, 48)
#define CMDQ_TLBI_1_LEAF		(1UL << 0)
#define CMDQ_TLBI_1_VA_MASK		BIT_MASK(63, 12)
#define CMDQ_TLBI_1_IPA_MASK		BIT_MASK(51, 12)

#define CMDQ_PRI_0_SSID			BIT_MASK(31, 12)
#define CMDQ_PRI_0_SID			BIT_MASK(63, 32)
#define CMDQ_PRI_1_GRPID		BIT_MASK(8, 0)
#define CMDQ_PRI_1_RESP			BIT_MASK(13, 12)

#define CMDQ_SYNC_0_CS			BIT_MASK(13, 12)
#define CMDQ_SYNC_0_CS_NONE		0
#define CMDQ_SYNC_0_CS_IRQ		1
#define CMDQ_SYNC_0_CS_SEV		2
#define CMDQ_SYNC_0_MSH			BIT_MASK(23, 22)
#define CMDQ_SYNC_0_MSIATTR		BIT_MASK(27, 24)
#define CMDQ_SYNC_0_MSIDATA		BIT_MASK(63, 32)
#define CMDQ_SYNC_1_MSIADDR_MASK	BIT_MASK(51, 2)

/* Event queue */
#define EVTQ_ENT_DWORDS			4
#define EVTQ_MAX_SZ_SHIFT		7

#define EVTQ_0_ID			BIT_MASK(7, 0)

#define ARM_SMMU_SYNC_TIMEOUT		1000000

#define FIELD_PREP(mask, val)	\
			(((u64)(val) << (__builtin_ffsl((mask)) - 1)) & (mask))
#define FIELD_GET(mask, reg)	\
			(((reg) & (mask)) >> (__builtin_ffsl((mask)) - 1))
#define FIELD_CLEAR(mask, reg)	\
			((reg) & (~(mask)))

#define CMDQ_OP_PREFETCH_CFG	0x1
#define CMDQ_OP_PREFETCH_ADDR	0x2
#define CMDQ_OP_CFGI_STE	0x3
#define CMDQ_OP_CFGI_ALL	0x4
#define CMDQ_OP_TLBI_NH_ASID	0x11
#define CMDQ_OP_TLBI_NH_VA	0x12
#define CMDQ_OP_TLBI_EL2_ALL	0x20
#define CMDQ_OP_TLBI_S12_VMALL	0x28
#define CMDQ_OP_TLBI_S2_IPA	0x2a
#define CMDQ_OP_TLBI_NSNH_ALL	0x30
#define CMDQ_OP_CMD_SYNC	0x46
#define ARM_SMMU_FEAT_2_LVL_STRTAB	(1 << 0)

/* High-level queue structures */
struct arm_smmu_cmdq_ent {
	/* Common fields */
	u8				opcode;
	bool				substream_valid;

	/* Command-specific fields */
	union {
		struct {
			u32			sid;
			u8			size;
			u64			addr;
		} prefetch;

		struct {
			u32			sid;
			union {
				bool		leaf;
				u8		span;
			};
		} cfgi;

		struct {
			u16			asid;
			u16			vmid;
			bool			leaf;
			u64			addr;
		} tlbi;

		struct {
			u32			msidata;
			u64			msiaddr;
		} sync;
	};
};

struct arm_smmu_queue {
	u64	*base;
	u64	base_dma;
	u64	q_base;
	u64	ent_dwords;
	u32	max_n_shift;
	u32	prod;
	u32	cons;
	u32 	*prod_reg;
	u32 	*cons_reg;
	u32	gerr_mask;
};

struct arm_smmu_cmdq {
	struct arm_smmu_queue		q;
	spinlock_t			lock;
};

struct arm_smmu_evtq {
	struct arm_smmu_queue		q;
};

/* High-level stream table structures */
struct arm_smmu_strtab_l1_desc {
	u8	span;
	u64	*l2ptr;
	u64	l2ptr_dma;
	u32	active_stes;
};

struct arm_smmu_strtab_cfg {
	u64				*strtab;
	u64				strtab_dma;
	struct arm_smmu_strtab_l1_desc	*l1_desc;
	unsigned int			num_l1_ents;
	u64				strtab_base;
	u32				strtab_base_cfg;
};

/* An SMMUv3 instance */
struct arm_smmu_device {
	void				*base;
	u32				features;
	struct arm_smmu_cmdq		cmdq;
	struct arm_smmu_evtq		evtq;
	unsigned int			sid_bits;
	struct arm_smmu_strtab_cfg	strtab_cfg;
};

static struct arm_smmu_device smmu_devices[JAILHOUSE_MAX_IOMMU_UNITS];

/* Low-level queue manipulation functions */
static bool queue_full(struct arm_smmu_queue *q)
{
	u32 shift = q->max_n_shift;

	return Q_FULL(q->prod, q->cons, shift);
}

static bool queue_empty(struct arm_smmu_queue *q)
{
	u32 shift = q->max_n_shift;

	return Q_EMPTY(q->prod, q->cons, shift);
}

static void queue_sync_cons(struct arm_smmu_queue *q)
{
	q->cons = mmio_read32(q->cons_reg);
}

static bool queue_error(struct arm_smmu_device *smmu, struct arm_smmu_queue *q)
{
	u32 gerror, gerrorn;

	gerror = mmio_read32(smmu->base + ARM_SMMU_GERROR);
	gerrorn = mmio_read32(smmu->base + ARM_SMMU_GERRORN);

	return (gerror ^ gerrorn) & q->gerr_mask;
}

static void queue_inc_prod(struct arm_smmu_queue *q)
{
	u32 shift = q->max_n_shift;
	u32 prod = (Q_WRP(q->prod, shift) | Q_IDX(q->prod, shift)) + 1;

	q->prod = Q_OVF(q->prod) | Q_WRP(prod, shift) | Q_IDX(prod, shift);
	mmio_write32(q->prod_reg, q->prod);
}

static void queue_write(u64 *dst, u64 *src, u32 n_dwords)
{
	u32 n;

	for (n = 0; n < n_dwords; ++n)
		*dst++ = *src++;
	dsb(ishst);
}

static u64 *queue_entry(struct arm_smmu_queue *q, u32 reg)
{
	return q->base + (Q_IDX(reg, q->max_n_shift) * q->ent_dwords);
}

/* High-level queue accessors */
static int arm_smmu_cmdq_build_cmd(u64 *cmd, struct arm_smmu_cmdq_ent *ent)
{
	memset(cmd, 0, CMDQ_ENT_SIZE);
	cmd[0] |= FIELD_PREP(CMDQ_0_OP, ent->opcode);

	switch (ent->opcode) {
	case CMDQ_OP_TLBI_EL2_ALL:
	case CMDQ_OP_TLBI_NSNH_ALL:
		break;
	case CMDQ_OP_PREFETCH_ADDR:
		cmd[1] |= FIELD_PREP(CMDQ_PREFETCH_1_SIZE, ent->prefetch.size);
		cmd[1] |= ent->prefetch.addr & CMDQ_PREFETCH_1_ADDR_MASK;
		/* Fallthrough */
	case CMDQ_OP_PREFETCH_CFG:
		cmd[0] |= FIELD_PREP(CMDQ_PREFETCH_0_SID, ent->prefetch.sid);
		break;
	case CMDQ_OP_CFGI_STE:
		cmd[0] |= FIELD_PREP(CMDQ_CFGI_0_SID, ent->cfgi.sid);
		cmd[1] |= FIELD_PREP(CMDQ_CFGI_1_LEAF, ent->cfgi.leaf);
		break;
	case CMDQ_OP_CFGI_ALL:
		/* Cover the entire SID range */
		cmd[1] |= FIELD_PREP(CMDQ_CFGI_1_RANGE, 31);
		break;
	case CMDQ_OP_TLBI_NH_VA:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_ASID, ent->tlbi.asid);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_LEAF, ent->tlbi.leaf);
		cmd[1] |= ent->tlbi.addr & CMDQ_TLBI_1_VA_MASK;
		break;
	case CMDQ_OP_TLBI_S2_IPA:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_LEAF, ent->tlbi.leaf);
		cmd[1] |= ent->tlbi.addr & CMDQ_TLBI_1_IPA_MASK;
		break;
	case CMDQ_OP_TLBI_NH_ASID:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_ASID, ent->tlbi.asid);
		/* Fallthrough */
	case CMDQ_OP_TLBI_S12_VMALL:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		break;
	case CMDQ_OP_CMD_SYNC:
		if (ent->sync.msiaddr)
			cmd[0] |= FIELD_PREP(CMDQ_SYNC_0_CS, CMDQ_SYNC_0_CS_IRQ);
		else
			cmd[0] |= FIELD_PREP(CMDQ_SYNC_0_CS, CMDQ_SYNC_0_CS_SEV);
		cmd[0] |= FIELD_PREP(CMDQ_SYNC_0_MSH, ARM_SMMU_SH_ISH) |
			  FIELD_PREP(CMDQ_SYNC_0_MSIATTR, ARM_SMMU_MEMATTR_OIWB) |
			  FIELD_PREP(CMDQ_SYNC_0_MSIDATA, ent->sync.msidata);
		cmd[1] |= ent->sync.msiaddr & CMDQ_SYNC_1_MSIADDR_MASK;
		break;
	default:
		return trace_error(-ENOENT);
	}

	return 0;
}

static void arm_smmu_cmdq_skip_err(struct arm_smmu_device *smmu)
{
	struct arm_smmu_queue *q;
	u64 cmd[CMDQ_ENT_DWORDS];
	u32 gerrorn;
	struct arm_smmu_cmdq_ent cmd_sync = {
		.opcode = CMDQ_OP_CMD_SYNC,
	};

	q = &smmu->cmdq.q;

	/*
	 * Convert the faulty command to sync and clear the error so
	 * command consumption can continue.
	 */
	arm_smmu_cmdq_build_cmd(cmd, &cmd_sync);
	queue_write(queue_entry(q, q->cons), cmd, q->ent_dwords);

	gerrorn = mmio_read32(smmu->base + ARM_SMMU_GERRORN);

	gerrorn ^= GERROR_CMDQ_ERR;
	mmio_write32(smmu->base + ARM_SMMU_GERRORN, gerrorn);
}

static void arm_smmu_cmdq_insert_cmd(struct arm_smmu_device *smmu, u64 *cmd)
{
	struct arm_smmu_queue *q = &smmu->cmdq.q;

	while (queue_full(q))
		queue_sync_cons(q);

	queue_write(queue_entry(q, q->prod), cmd, q->ent_dwords);
	queue_inc_prod(q);
	while (!queue_empty(q)) {
		queue_sync_cons(q);
		if (queue_error(smmu, q))
			arm_smmu_cmdq_skip_err(smmu);
	}
}

static void arm_smmu_cmdq_issue_cmd(struct arm_smmu_device *smmu,
				    struct arm_smmu_cmdq_ent *ent)
{
	u64 cmd[CMDQ_ENT_DWORDS];

	if (arm_smmu_cmdq_build_cmd(cmd, ent))
		/* Ignore any unknown command */
		return;

	spin_lock(&smmu->cmdq.lock);
	arm_smmu_cmdq_insert_cmd(smmu, cmd);
	spin_unlock(&smmu->cmdq.lock);
}

static void arm_smmu_cmdq_issue_sync(struct arm_smmu_device *smmu)
{
	struct arm_smmu_cmdq_ent ent = { .opcode = CMDQ_OP_CMD_SYNC };
	u64 cmd[CMDQ_ENT_DWORDS];

	arm_smmu_cmdq_build_cmd(cmd, &ent);

	spin_lock(&smmu->cmdq.lock);
	arm_smmu_cmdq_insert_cmd(smmu, cmd);
	spin_unlock(&smmu->cmdq.lock);
}

/* Stream table manipulation functions */
static void
arm_smmu_write_strtab_l1_desc(u64 *dst, struct arm_smmu_strtab_l1_desc *desc)
{
	u64 val = 0;

	val |= FIELD_PREP(STRTAB_L1_DESC_SPAN, desc->span);
	val |= desc->l2ptr_dma & STRTAB_L1_DESC_L2PTR_MASK;

	/* Assuming running on Little endian cpu */
	*dst = val;
	dsb(ishst);
}

static void arm_smmu_sync_ste_for_sid(struct arm_smmu_device *smmu, u32 sid)
{
	struct arm_smmu_cmdq_ent cmd = {
		.opcode	= CMDQ_OP_CFGI_STE,
		.cfgi	= {
			.sid	= sid,
			.leaf	= true,
		},
	};

	arm_smmu_cmdq_issue_cmd(smmu, &cmd);
	arm_smmu_cmdq_issue_sync(smmu);
}

static void arm_smmu_write_strtab_ent(struct arm_smmu_device *smmu, u32 sid,
				      u64 *guest_ste, u64 *dst, bool bypass,
				      u32 vmid)
{
	struct paging_structures *pg_structs = &this_cell()->arch.mm;
	u64 val, vttbr;

	val = 0;

	/* Bypass */
	if (bypass) {
		val = STRTAB_STE_0_V;
		val |= FIELD_PREP(STRTAB_STE_0_CFG, STRTAB_STE_0_CFG_BYPASS);
		dst[1] = FIELD_PREP(STRTAB_STE_1_SHCFG,
				    STRTAB_STE_1_SHCFG_INCOMING);
		dst[2] = FIELD_PREP(STRTAB_STE_2_S2VMID, vmid);
		dst[0] = val;
		dsb(ishst);
		if (smmu)
			arm_smmu_sync_ste_for_sid(smmu, sid);
		return;
	}

	dst[2] = FIELD_PREP(STRTAB_STE_2_S2VMID, vmid) |
		 FIELD_PREP(STRTAB_STE_2_VTCR, VTCR_CELL) |
		 STRTAB_STE_2_S2PTW | STRTAB_STE_2_S2AA64 |
		 STRTAB_STE_2_S2R;

	vttbr = paging_hvirt2phys(pg_structs->root_table);
	dst[3] = vttbr & STRTAB_STE_3_S2TTB_MASK;

	val |= FIELD_PREP(STRTAB_STE_0_CFG, STRTAB_STE_0_CFG_S2_TRANS);
	val |= STRTAB_STE_0_V;

	arm_smmu_sync_ste_for_sid(smmu, sid);
	dst[0] = val;
	dsb(ishst);
	arm_smmu_sync_ste_for_sid(smmu, sid);
}

static void arm_smmu_init_bypass_stes(u64 *strtab, unsigned int nent)
{
	unsigned int n;

	for (n = 0; n < nent; ++n) {
		arm_smmu_write_strtab_ent(NULL, -1, NULL, strtab, true,
					  (u32)this_cell()->config->id);
		strtab += STRTAB_STE_DWORDS;
	}
}

static int arm_smmu_init_strtab_linear(struct arm_smmu_device *smmu)
{
	void *strtab;
	u64 reg;
	u32 size;
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;

	size = (1 << smmu->sid_bits) * STRTAB_STE_SIZE;
	strtab = page_alloc_aligned(&mem_pool, PAGES(size));
	if (!strtab)
		return -ENOMEM;

	cfg->strtab_dma = paging_hvirt2phys(strtab);
	cfg->strtab = strtab;
	cfg->num_l1_ents = 1 << smmu->sid_bits;

	/* Configure strtab_base_cfg for a linear table covering all SIDs */
	reg  = FIELD_PREP(STRTAB_BASE_CFG_FMT, STRTAB_BASE_CFG_FMT_LINEAR);
	reg |= FIELD_PREP(STRTAB_BASE_CFG_LOG2SIZE, smmu->sid_bits);
	cfg->strtab_base_cfg = reg;

	arm_smmu_init_bypass_stes(strtab, cfg->num_l1_ents);
	return 0;
}

static int arm_smmu_init_l1_strtab(struct arm_smmu_device *smmu)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	u32 size = sizeof(*cfg->l1_desc) * cfg->num_l1_ents;
	void *strtab = smmu->strtab_cfg.strtab;
	unsigned int n;

	cfg->l1_desc = page_alloc(&mem_pool, PAGES(size));
	if (!cfg->l1_desc)
		return -ENOMEM;

	for (n = 0; n < cfg->num_l1_ents; ++n) {
		memset(&cfg->l1_desc[n], 0, sizeof(*cfg->l1_desc));
		arm_smmu_write_strtab_l1_desc(strtab, &cfg->l1_desc[n]);
		strtab += STRTAB_L1_DESC_SIZE;
	}

	return 0;
}

static int arm_smmu_init_strtab_2lvl(struct arm_smmu_device *smmu)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	u32 size, l1size;
	void *strtab;
	u64 reg;
	int ret;

	/* Calculate the L1 size, capped to the SIDSIZE. */
	size = STRTAB_L1_SZ_SHIFT - 3;
	size = MIN(size, smmu->sid_bits - STRTAB_SPLIT);
	cfg->num_l1_ents = 1 << size;
	size += STRTAB_SPLIT;

	/*
	 * This driver only supports "size" bits out of "smmu->sid_bits"
	 * that is specified by hardware.
	 */
	if (size < smmu->sid_bits) {
		printk("WARN: SMMU 2-level strtab only covers %u/%u bits of SID\n",
		       size, smmu->sid_bits);
		smmu->sid_bits = size;
	}

	l1size = cfg->num_l1_ents * STRTAB_L1_DESC_SIZE;
	strtab = page_alloc_aligned(&mem_pool, PAGES(l1size));
	if (!strtab)
		return -ENOMEM;

	cfg->strtab_dma = paging_hvirt2phys(strtab);
	cfg->strtab = strtab;

	/* Configure strtab_base_cfg for 2 levels */
	reg  = FIELD_PREP(STRTAB_BASE_CFG_FMT, STRTAB_BASE_CFG_FMT_2LVL);
	reg |= FIELD_PREP(STRTAB_BASE_CFG_LOG2SIZE, size);
	reg |= FIELD_PREP(STRTAB_BASE_CFG_SPLIT, STRTAB_SPLIT);
	cfg->strtab_base_cfg = reg;

	ret = arm_smmu_init_l1_strtab(smmu);
	if (ret)
		page_free(&mem_pool, strtab, PAGES(l1size));

	return ret;
}

static int arm_smmu_init_strtab(struct arm_smmu_device *smmu)
{
	u64 reg;
	int ret;

	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB)
		ret = arm_smmu_init_strtab_2lvl(smmu);
	else
		ret = arm_smmu_init_strtab_linear(smmu);

	if (ret)
		return ret;

	/* Set the strtab base address */
	reg  = smmu->strtab_cfg.strtab_dma & STRTAB_BASE_ADDR_MASK;
	reg |= STRTAB_BASE_RA;
	smmu->strtab_cfg.strtab_base = reg;

	return 0;
}

static int arm_smmu_init_one_queue(struct arm_smmu_device *smmu,
				   struct arm_smmu_queue *q,
				   unsigned long prod_off,
				   unsigned long cons_off,
				   unsigned long dwords,
				   unsigned int gerr_mask)
{
	/* Queue size is capped to 4K. So allocate 1 page */
	q->base = page_alloc(&mem_pool, 1);
	if (!q->base)
		return -ENOMEM;

	q->base_dma = paging_hvirt2phys(q->base);

	q->prod_reg	= smmu->base + prod_off;
	q->cons_reg	= smmu->base + cons_off;
	q->ent_dwords	= dwords;

	q->q_base  = Q_BASE_RWA;
	q->q_base |= q->base_dma & Q_BASE_ADDR_MASK;
	q->q_base |= FIELD_PREP(Q_BASE_LOG2SIZE, q->max_n_shift);

	q->gerr_mask = gerr_mask;
	q->prod = q->cons = 0;

	return 0;
}

static int arm_smmu_init_queues(struct arm_smmu_device *smmu)
{
	int ret;

	/* cmdq */
	ret = arm_smmu_init_one_queue(smmu, &smmu->cmdq.q, ARM_SMMU_CMDQ_PROD,
				      ARM_SMMU_CMDQ_CONS, CMDQ_ENT_DWORDS,
				      GERROR_CMDQ_ERR);
	if (ret)
		return ret;

	/* evtq */
	ret = arm_smmu_init_one_queue(smmu, &smmu->evtq.q, ARM_SMMU_EVTQ_PROD,
				      ARM_SMMU_EVTQ_CONS, EVTQ_ENT_DWORDS,
				      GERROR_EVTQ_ABT_ERR);
	if (ret)
		return ret;

	return ret;
}

static int arm_smmu_init_structures(struct arm_smmu_device *smmu)
{
	int ret;

	ret = arm_smmu_init_queues(smmu);
	if (ret)
		return ret;

	return arm_smmu_init_strtab(smmu);
}

static int arm_smmu_write_reg_sync(struct arm_smmu_device *smmu, u32 val,
				   unsigned int reg_off, unsigned int ack_off)
{
	u32 n, timeout = ARM_SMMU_SYNC_TIMEOUT;

	mmio_write32(smmu->base + reg_off, val);
	for (n = 0; n < timeout; n++) {
		if (mmio_read32(smmu->base + ack_off) == val)
			return 0;
	}

	return trace_error(-EINVAL);
}

static int arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	int ret;
	u32 reg, enables;
	struct arm_smmu_cmdq_ent cmd;

	/* Clear CR0 and sync (disables SMMU and queue processing) */
	ret = arm_smmu_write_reg_sync(smmu, 0, ARM_SMMU_CR0, ARM_SMMU_CR0ACK);
	if (ret)
		return ret;

	/* CR1 (table and queue memory attributes) */
	reg = FIELD_PREP(CR1_TABLE_SH, ARM_SMMU_SH_ISH) |
	      FIELD_PREP(CR1_TABLE_OC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_TABLE_IC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_QUEUE_SH, ARM_SMMU_SH_ISH) |
	      FIELD_PREP(CR1_QUEUE_OC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_QUEUE_IC, CR1_CACHE_WB);
	mmio_write32(smmu->base + ARM_SMMU_CR1, reg);

	/* Stream table */
	mmio_write64(smmu->base + ARM_SMMU_STRTAB_BASE,
		     smmu->strtab_cfg.strtab_base);
	mmio_write32(smmu->base + ARM_SMMU_STRTAB_BASE_CFG,
		     smmu->strtab_cfg.strtab_base_cfg);

	/* Command queue */
	mmio_write64(smmu->base + ARM_SMMU_CMDQ_BASE, smmu->cmdq.q.q_base);
	mmio_write32(smmu->base + ARM_SMMU_CMDQ_PROD, smmu->cmdq.q.prod);
	mmio_write32(smmu->base + ARM_SMMU_CMDQ_CONS, smmu->cmdq.q.cons);

	enables = CR0_CMDQEN;
	ret = arm_smmu_write_reg_sync(smmu, enables, ARM_SMMU_CR0,
				      ARM_SMMU_CR0ACK);
	if (ret)
		return ret;

	/* Invalidate any cached configuration */
	cmd.opcode = CMDQ_OP_CFGI_ALL;
	arm_smmu_cmdq_issue_cmd(smmu, &cmd);
	arm_smmu_cmdq_issue_sync(smmu);

	cmd.opcode = CMDQ_OP_TLBI_NSNH_ALL;
	arm_smmu_cmdq_issue_cmd(smmu, &cmd);

	/* Invalidate any stale TLB entries */
	cmd.opcode = CMDQ_OP_TLBI_EL2_ALL;
	arm_smmu_cmdq_issue_cmd(smmu, &cmd);
	arm_smmu_cmdq_issue_sync(smmu);

	/* Event queue */
	mmio_write64(smmu->base + ARM_SMMU_EVTQ_BASE, smmu->evtq.q.q_base);
	mmio_write32(smmu->base + ARM_SMMU_EVTQ_PROD, smmu->evtq.q.prod);
	mmio_write32(smmu->base + ARM_SMMU_EVTQ_CONS, smmu->evtq.q.cons);

	enables |= CR0_EVTQEN;
	ret = arm_smmu_write_reg_sync(smmu, enables, ARM_SMMU_CR0,
				      ARM_SMMU_CR0ACK);
	if (ret)
		return ret;

	/* ToDo: Add support for PRI queue and IRQs  */

	enables |= CR0_SMMUEN;
	ret = arm_smmu_write_reg_sync(smmu, enables, ARM_SMMU_CR0,
				      ARM_SMMU_CR0ACK);

	return ret;
}

static int arm_smmu_device_init_features(struct arm_smmu_device *smmu)
{
	u32 reg;

	/* IDR0 */
	reg = mmio_read32(smmu->base + ARM_SMMU_IDR0);

	smmu->features = 0;
	/* 2-level structures */
	if (FIELD_GET(IDR0_ST_LVL, reg) == IDR0_ST_LVL_2LVL)
		smmu->features |= ARM_SMMU_FEAT_2_LVL_STRTAB;

	if (!(reg & IDR0_S2P))
		return trace_error(-ENODEV);

	if (FIELD_GET(IDR0_S1P, reg))
		smmu->features |= IDR0_S1P;

	if (FIELD_GET(IDR0_VMID16, reg))
		smmu->features |= IDR0_VMID16;

	/* IDR1 */
	reg = mmio_read32(smmu->base + ARM_SMMU_IDR1);
	if (reg & (IDR1_TABLES_PRESET | IDR1_QUEUES_PRESET | IDR1_REL))
		return trace_error(-ENODEV);

	/* Queue sizes, capped at 4k */
	smmu->cmdq.q.max_n_shift = MIN(CMDQ_MAX_SZ_SHIFT,
				       FIELD_GET(IDR1_CMDQS, reg));
	if (!smmu->cmdq.q.max_n_shift)
		return trace_error(-ENODEV);

	smmu->evtq.q.max_n_shift = MIN(EVTQ_MAX_SZ_SHIFT,
				       FIELD_GET(IDR1_EVTQS, reg));

	/* SID sizes */
	smmu->sid_bits = FIELD_GET(IDR1_SIDSIZE, reg);

	/*
	 * If the SMMU supports fewer bits than would fill a single L2 stream
	 * table, use a linear table instead.
	 */
	if (smmu->sid_bits <= STRTAB_SPLIT)
		smmu->features &= ~ARM_SMMU_FEAT_2_LVL_STRTAB;

	return 0;
}

static int arm_smmu_init_l2_strtab(struct arm_smmu_device *smmu, u32 sid)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	struct arm_smmu_strtab_l1_desc *desc;
	struct arm_smmu_cmdq_ent cmd;
	void *strtab;
	u32 size;

	desc = &cfg->l1_desc[sid >> STRTAB_SPLIT];
	if (desc->l2ptr) {
		desc->active_stes++;
		return 0;
	}

	size = 1 << (STRTAB_SPLIT + STRTAB_STE_DWORDS_BITS + 3);
	strtab = &cfg->strtab[(sid >> STRTAB_SPLIT) * STRTAB_L1_DESC_DWORDS];

	desc->span = STRTAB_SPLIT + 1;
	desc->l2ptr = page_alloc_aligned(&mem_pool, PAGES(size));
	if (!desc->l2ptr)
		return trace_error(-ENOMEM);

	desc->l2ptr_dma = paging_hvirt2phys(desc->l2ptr);
	desc->active_stes = 1;
	arm_smmu_init_bypass_stes(desc->l2ptr, 1 << STRTAB_SPLIT);
	arm_smmu_write_strtab_l1_desc(strtab, desc);

	/* Invalidate cached L1 descriptors. */
	cmd.opcode = CMDQ_OP_CFGI_STE;
	cmd.cfgi.sid = sid;
	cmd.cfgi.leaf = false;
	arm_smmu_cmdq_issue_cmd(smmu, &cmd);

	return 0;
}

static void arm_smmu_uninit_l2_strtab(struct arm_smmu_device *smmu, u32 sid)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	struct arm_smmu_strtab_l1_desc *desc;
	struct arm_smmu_cmdq_ent cmd;
	void *strtab;
	u32 size;

	desc = &cfg->l1_desc[sid >> STRTAB_SPLIT];

	desc->active_stes--;
	if (desc->active_stes)
		return;

	desc->l2ptr = NULL;
	desc->l2ptr_dma = 0;
	desc->span = 0;
	strtab = &cfg->strtab[(sid >> STRTAB_SPLIT) * STRTAB_L1_DESC_DWORDS];
	arm_smmu_write_strtab_l1_desc(strtab, desc);

	/* Invalidate cached L1 descriptors. */
	cmd.opcode = CMDQ_OP_CFGI_STE;
	cmd.cfgi.sid = sid;
	cmd.cfgi.leaf = false;
	arm_smmu_cmdq_issue_cmd(smmu, &cmd);

	size = 1 << (STRTAB_SPLIT + STRTAB_STE_DWORDS_BITS + 3);
	page_free(&mem_pool, desc->l2ptr, PAGES(size));
}

static u64 *arm_smmu_get_step_for_sid(struct arm_smmu_device *smmu, u32 sid)
{
	u64 *step;
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;

	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB) {
		struct arm_smmu_strtab_l1_desc *l1_desc;
		int idx;

		/* Two-level walk */
		idx = (sid >> STRTAB_SPLIT) * STRTAB_L1_DESC_DWORDS;
		l1_desc = &cfg->l1_desc[idx];
		idx = (sid & ((1 << STRTAB_SPLIT) - 1)) * STRTAB_STE_DWORDS;
		step = &l1_desc->l2ptr[idx];
	} else {
		/* Simple linear lookup */
		step = &cfg->strtab[sid * STRTAB_STE_DWORDS];
	}

	return step;
}

static int arm_smmu_init_ste(struct arm_smmu_device *smmu, u32 sid, u32 vmid)
{
	int ret = 0;
	u64 *step;

	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB) {
		ret = arm_smmu_init_l2_strtab(smmu, sid);
		if (ret)
			return ret;
	}

	step = arm_smmu_get_step_for_sid(smmu, sid);
	arm_smmu_write_strtab_ent(smmu, sid, NULL, step, false, vmid);

	return 0;
}

static void arm_smmu_uninit_ste(struct arm_smmu_device *smmu, u32 sid, u32 vmid)
{
	u64 *step;

	step = arm_smmu_get_step_for_sid(smmu, sid);
	arm_smmu_write_strtab_ent(smmu, sid, NULL, step, true, vmid);

	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB)
		arm_smmu_uninit_l2_strtab(smmu, sid);
}

static int arm_smmuv3_cell_init(struct cell *cell)
{
	struct arm_smmu_device *smmu = &smmu_devices[0];
	struct jailhouse_iommu *iommu;
	struct arm_smmu_cmdq_ent cmd;
	union jailhouse_stream_id sid;
	unsigned int n, s;
	int ret;

	if (!iommu_count_units())
		return 0;

	iommu = &system_config->platform_info.iommu_units[0];
	for (n = 0; n < iommu_count_units(); iommu++, smmu++, n++) {
		if (iommu->type != JAILHOUSE_IOMMU_SMMUV3)
			continue;

		for_each_stream_id(sid, cell->config, s) {
			ret = arm_smmu_init_ste(smmu, sid.id, cell->config->id);
			if (ret)
				return ret;
		}

		cmd.opcode	= CMDQ_OP_TLBI_S12_VMALL;
		cmd.tlbi.vmid	= cell->config->id;
		arm_smmu_cmdq_issue_cmd(smmu, &cmd);
		arm_smmu_cmdq_issue_sync(smmu);
	}

	return 0;
}

static void arm_smmuv3_cell_exit(struct cell *cell)
{
	struct arm_smmu_device *smmu = &smmu_devices[0];
	struct jailhouse_iommu *iommu;
	struct arm_smmu_cmdq_ent cmd;
	union jailhouse_stream_id sid;
	unsigned int n, s;

	if (!iommu_count_units())
		return;

	iommu = &system_config->platform_info.iommu_units[0];
	for (n = 0; n < JAILHOUSE_MAX_IOMMU_UNITS; iommu++, smmu++, n++) {
		if (iommu->type != JAILHOUSE_IOMMU_SMMUV3)
			continue;

		for_each_stream_id(sid, cell->config, s) {
			arm_smmu_uninit_ste(smmu, sid.id, cell->config->id);
		}

		cmd.opcode	= CMDQ_OP_TLBI_S12_VMALL;
		cmd.tlbi.vmid	= cell->config->id;
		arm_smmu_cmdq_issue_cmd(smmu, &cmd);
		arm_smmu_cmdq_issue_sync(smmu);
	}
}

static int arm_smmuv3_init(void)
{
	struct arm_smmu_device *smmu = &smmu_devices[0];
	struct jailhouse_iommu *iommu;
	int ret;
	unsigned int n;

	iommu = &system_config->platform_info.iommu_units[0];
	for (n = 0; n < iommu_count_units(); iommu++, smmu++, n++) {
		if (iommu->type != JAILHOUSE_IOMMU_SMMUV3)
			continue;

		smmu->base = paging_map_device(iommu->base, iommu->size);

		/* ToDo: irq allocation*/

		ret = arm_smmu_device_init_features(smmu);
		if (ret)
			return ret;

		ret = arm_smmu_init_structures(smmu);
		if (ret)
			return ret;

		/* Reset the device */
		ret = arm_smmu_device_reset(smmu);
		if (ret)
			return ret;
	}

	return arm_smmuv3_cell_init(&root_cell);
}

DEFINE_UNIT_MMIO_COUNT_REGIONS_STUB(arm_smmuv3);
DEFINE_UNIT_SHUTDOWN_STUB(arm_smmuv3);
DEFINE_UNIT(arm_smmuv3, "ARM SMMU v3");
