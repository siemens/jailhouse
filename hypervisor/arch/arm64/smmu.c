/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright 2018-2020 NXP
 * Copyright Siemens AG, 2020
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Modified from Linux smmu.c
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/unit.h>
#include <asm/iommu.h>

#include <jailhouse/cell-config.h>

#define TLB_LOOP_TIMEOUT		1000000

/* SMMU global address space */
#define ARM_SMMU_GR0(smmu)		((smmu)->base)
#define ARM_SMMU_GR1(smmu)		((smmu)->base + (1 << (smmu)->pgshift))

/* Translation context bank */
#define ARM_SMMU_CB(smmu, n)	((smmu)->cb_base + ((n) << (smmu)->pgshift))

/* Configuration Register 0 */
#define ARM_SMMU_GR0_sCR0		0x0
#define sCR0_CLIENTPD			(1 << 0)
#define sCR0_GFRE			(1 << 1)
#define sCR0_GFIE			(1 << 2)
#define sCR0_GCFGFRE			(1 << 4)
#define sCR0_GCFGFIE			(1 << 5)
#define sCR0_USFCFG			(1 << 10)
#define sCR0_VMIDPNE			(1 << 11)
#define sCR0_PTM			(1 << 12)
#define sCR0_FB				(1 << 13)

/* Auxiliary Configuration Register */
#define ARM_SMMU_GR0_sACR		0x10
#define ARM_MMU500_ACTLR_CPRE		(1 << 1)
#define ARM_MMU500_ACR_SMTNMB_TLBEN	(1 << 8)
#define ARM_MMU500_ACR_S2CRB_TLBEN	(1 << 10)
#define ARM_MMU500_ACR_CACHE_LOCK	(1 << 26)

/* Identification registers */
#define ARM_SMMU_GR0_ID0		0x20
#define ARM_SMMU_GR0_ID1		0x24
#define ARM_SMMU_GR0_ID2		0x28
#define ARM_SMMU_GR0_ID7		0x3c
#define ID0_S1TS			(1 << 30)
#define ID0_S2TS			(1 << 29)
#define ID0_NTS				(1 << 28)
#define ID0_SMS				(1 << 27)
#define ID0_PTFS_NO_AARCH32		(1 << 25)
#define ID0_PTFS_NO_AARCH32S		(1 << 24)
#define ID0_CTTW			(1 << 14)
#define ID0_NUMSIDB_SHIFT		9
#define ID0_NUMSIDB_MASK		0xf
#define ID0_EXIDS			(1 << 8)
#define ID0_NUMSMRG_SHIFT		0
#define ID0_NUMSMRG_MASK		0xff

#define ID1_PAGESIZE			(1 << 31)
#define ID1_NUMPAGENDXB_SHIFT		28
#define ID1_NUMPAGENDXB_MASK		7
#define ID1_NUMS2CB_SHIFT		16
#define ID1_NUMS2CB_MASK		0xff
#define ID1_NUMCB_SHIFT			0
#define ID1_NUMCB_MASK			0xff

#define ID2_IAS_SHIFT			0
#define ID2_IAS_MASK			0xf
#define ID2_OAS_SHIFT			4
#define ID2_OAS_MASK			0xf
#define ID2_PTFS_4K			(1 << 12)

#define ID7_MAJOR_SHIFT			4
#define ID7_MAJOR_MASK			0xf

/* Global Fault Status Register */
#define ARM_SMMU_GR0_sGFSR		0x48

/* TLB */
#define ARM_SMMU_GR0_TLBIVMID		0X64
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70
#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define sTLBGSTATUS_GSACTIVE		(1 << 0)

/* Stream Match Register */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_VALID			(1 << 31)
#define SMR_MASK_SHIFT			16
#define SMR_ID_SHIFT			0

/* Stream-to-Context Register */
#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_EXIDVALID			(1 << 10)

/* Context Bank Attribute Registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define CBAR_VMID_SHIFT			0
#define CBAR_TYPE_SHIFT			16
#define CBAR_TYPE_S2_TRANS		(0 << CBAR_TYPE_SHIFT)

#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))
#define CBA2R_RW64_64BIT		(1 << 0)

/* Stage 1 translation context bank address space */
#define ARM_SMMU_CB_SCTLR		0x0
#define ARM_SMMU_CB_ACTLR		0x4
#define ARM_SMMU_CB_TTBR0		0x20
#define ARM_SMMU_CB_TTBCR		0x30
#define ARM_SMMU_CB_CONTEXTIDR		0x34
#define ARM_SMMU_CB_FSR			0x58

#define SCTLR_CFIE			(1 << 6)
#define SCTLR_CFRE			(1 << 5)
#define SCTLR_AFE			(1 << 2)
#define SCTLR_TRE			(1 << 1)
#define SCTLR_M				(1 << 0)

#define TCR_RES0			(BIT_MASK(31, 23) | BIT_MASK(20, 19))

#define FSR_MULTI			(1 << 31)
#define FSR_SS				(1 << 30)
#define FSR_UUT				(1 << 8)
#define FSR_ASF				(1 << 7)
#define FSR_TLBLKF			(1 << 6)
#define FSR_TLBMCF			(1 << 5)
#define FSR_EF				(1 << 4)
#define FSR_PF				(1 << 3)
#define FSR_AFF				(1 << 2)
#define FSR_TF				(1 << 1)
#define FSR_IGN				(FSR_AFF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)

/* Context Bank Index */
#define S2CR_CBNDX(s2cr)		SET_FIELD((s2cr), 7, 0)
/*  Register type */
#define S2CR_TYPE(s2cr)			SET_FIELD((s2cr), 17, 16)
/* Privileged Attribute Configuration */
#define S2CR_PRIVCFG(s2cr)		SET_FIELD((s2cr), 25, 24)

#define S2CR_TYPE_TRANS			0
#define S2CR_TYPE_FAULT			2

#define S2CR_PRIVCFG_DEFAULT		0

struct arm_smmu_smr {
	u16				mask;
	u16				id;
	bool				valid;
};

struct arm_smmu_cfg {
	unsigned int			id;
	u32				cbar;
};
struct arm_smmu_cb {
	u64				ttbr;
	u32				tcr[2];
	u32				mair[2];
	struct arm_smmu_cfg		*cfg;
};

struct arm_smmu_device {
	void	*base;
	void	*cb_base;
	u32	num_masters;
	unsigned long			pgshift;
	u32				num_context_banks;
	u32				num_s2_context_banks;
	struct arm_smmu_cb		*cbs;
	u32				num_mapping_groups;
	u16				arm_sid_mask;
	struct arm_smmu_smr		*smrs;
	struct arm_smmu_cfg		*cfgs;
	unsigned long			ipa_size;
	unsigned long			pa_size;
	u32				num_global_irqs;
	unsigned int			*irqs;
};

static struct arm_smmu_device smmu_device[JAILHOUSE_MAX_IOMMU_UNITS];
static unsigned int num_smmu_devices;

#define for_each_smmu(smmu, counter)				\
	for ((smmu) = &smmu_device[0], (counter) = 0;		\
	     (counter) < num_smmu_devices;			\
	     (smmu)++, (counter)++)

static void arm_smmu_write_smr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_smr *smr = smmu->smrs + idx;
	u32 reg = (smr->id << SMR_ID_SHIFT) | (smr->mask << SMR_MASK_SHIFT) |
		(smr->valid ? SMR_VALID : 0);

	mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_SMR(idx), reg);
}

static void arm_smmu_write_s2cr(struct arm_smmu_device *smmu, int idx,
				unsigned int type, unsigned int cbndx)
{
	u32 reg = S2CR_TYPE(type) | S2CR_CBNDX(cbndx) |
		  S2CR_PRIVCFG(S2CR_PRIVCFG_DEFAULT);

	mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_S2CR(idx), reg);
}

/* Wait for any pending TLB invalidations to complete */
static int arm_smmu_tlb_sync_global(struct arm_smmu_device *smmu)
{
	void *base = ARM_SMMU_GR0(smmu);
	unsigned int delay, i;

	mmio_write32(base + ARM_SMMU_GR0_sTLBGSYNC, 0);
	for (delay = 1; delay < TLB_LOOP_TIMEOUT; delay *= 2) {
		if (!(mmio_read32(base + ARM_SMMU_GR0_sTLBGSTATUS) &
		      sTLBGSTATUS_GSACTIVE))
			return 0;
		for (i = 0; i < 10 * 1000000; i++)
			cpu_relax();
	}
	printk("TLB sync timed out -- SMMU may be deadlocked\n");

	return trace_error(-EINVAL);
}

static int arm_smmu_init_context_bank(struct arm_smmu_device *smmu,
				      struct arm_smmu_cfg *cfg,
				      struct cell *cell)
{
	struct arm_smmu_cb *cb = &smmu->cbs[cfg->id];
	struct paging_structures *pg_structs;
	unsigned long cell_table;

	cb->cfg = cfg;

	cb->tcr[0] = VTCR_CELL & ~TCR_RES0;

	pg_structs = &cell->arch.mm;
	cell_table = paging_hvirt2phys(pg_structs->root_table);
	u64 vttbr = 0;

	vttbr |= (u64)cell->config->id << VTTBR_VMID_SHIFT;
	vttbr |= (u64)(cell_table & TTBR_MASK);
	cb->ttbr = vttbr;

	return 0;
}

static void arm_smmu_write_context_bank(struct arm_smmu_device *smmu, int idx)
{
	void *cb_base, *gr1_base;
	struct arm_smmu_cb *cb = &smmu->cbs[idx];
	struct arm_smmu_cfg *cfg = cb->cfg;
	u32 reg;

	cb_base = ARM_SMMU_CB(smmu, idx);

	/* Unassigned context banks only need disabling */
	if (!cfg) {
		mmio_write32(cb_base + ARM_SMMU_CB_SCTLR, 0);
		return;
	}

	gr1_base = ARM_SMMU_GR1(smmu);

	/* CBA2R */
	reg = CBA2R_RW64_64BIT;

	mmio_write32(gr1_base + ARM_SMMU_GR1_CBA2R(idx), reg);

	/* CBAR */
	reg = cfg->cbar;
	reg |= cfg->id << CBAR_VMID_SHIFT;
	mmio_write32(gr1_base + ARM_SMMU_GR1_CBAR(idx), reg);

	/*
	 * TTBCR
	 * We must write this before the TTBRs, since it determines the
	 * access behaviour of some fields (in particular, ASID[15:8]).
	 */
	mmio_write32(cb_base + ARM_SMMU_CB_TTBCR, cb->tcr[0]);

	/* TTBRs */
	mmio_write64(cb_base + ARM_SMMU_CB_TTBR0, cb->ttbr);

	/* SCTLR */
	mmio_write32(cb_base + ARM_SMMU_CB_SCTLR,
		     SCTLR_CFIE | SCTLR_CFRE | SCTLR_AFE | SCTLR_TRE | SCTLR_M);
}

static int arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	void *gr0_base = ARM_SMMU_GR0(smmu);
	unsigned int idx;
	u32 reg, major;
	int ret;

	/* Clear global FSR */
	reg = mmio_read32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_sGFSR);
	mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_sGFSR, reg);

	/*
	 * Reset stream mapping groups: Initial values mark all SMRn as
	 * invalid and all S2CRn as fault until overridden.
	 */
	for (idx = 0; idx < smmu->num_mapping_groups; ++idx) {
		if (smmu->smrs)
			arm_smmu_write_smr(smmu, idx);

		arm_smmu_write_s2cr(smmu, idx, S2CR_TYPE_FAULT, 0);
	}

	/*
	 * Before clearing ARM_MMU500_ACTLR_CPRE, need to
	 * clear CACHE_LOCK bit of ACR first. And, CACHE_LOCK
	 * bit is only present in MMU-500r2 onwards.
	 */
	reg = mmio_read32(gr0_base + ARM_SMMU_GR0_ID7);
	major = (reg >> ID7_MAJOR_SHIFT) & ID7_MAJOR_MASK;
	reg = mmio_read32(gr0_base + ARM_SMMU_GR0_sACR);
	if (major >= 2)
		reg &= ~ARM_MMU500_ACR_CACHE_LOCK;
	/*
	 * Allow unmatched Stream IDs to allocate bypass
	 * TLB entries for reduced latency.
	 */
	reg |= ARM_MMU500_ACR_SMTNMB_TLBEN | ARM_MMU500_ACR_S2CRB_TLBEN;
	mmio_write32(gr0_base + ARM_SMMU_GR0_sACR, reg);

	/* Make sure all context banks are disabled and clear CB_FSR */
	for (idx = 0; idx < smmu->num_context_banks; ++idx) {
		void *cb_base = ARM_SMMU_CB(smmu, idx);

		arm_smmu_write_context_bank(smmu, idx);
		mmio_write32(cb_base + ARM_SMMU_CB_FSR, FSR_FAULT);
		/*
		 * Disable MMU-500's not-particularly-beneficial next-page
		 * prefetcher for the sake of errata #841119 and #826419.
		 */
		reg = mmio_read32(cb_base + ARM_SMMU_CB_ACTLR);
		reg &= ~ARM_MMU500_ACTLR_CPRE;
		mmio_write32(cb_base + ARM_SMMU_CB_ACTLR, reg);
	}

	/* Invalidate the TLB, just in case */
	mmio_write32(gr0_base + ARM_SMMU_GR0_TLBIALLH, 0);
	mmio_write32(gr0_base + ARM_SMMU_GR0_TLBIALLNSNH, 0);

	/* Enable fault reporting */
	reg = sCR0_GFRE | sCR0_GFIE | sCR0_GCFGFRE | sCR0_GCFGFIE;

	/* Private VMIDS, disable TLB broadcasting, fault unmatched streams */
	reg |= sCR0_VMIDPNE | sCR0_PTM | sCR0_USFCFG;

	/* Push the button */
	ret = arm_smmu_tlb_sync_global(smmu);
	mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_sCR0, reg);

	return ret;
}

static int arm_smmu_id_size_to_bits(int size)
{
	switch (size) {
	case 0:
		return 32;
	case 1:
		return 36;
	case 2:
		return 40;
	case 3:
		return 42;
	case 4:
		return 44;
	case 5:
	default:
		return 48;
	}
}

static int arm_smmu_device_cfg_probe(struct arm_smmu_device *smmu)
{
	void *gr0_base = ARM_SMMU_GR0(smmu);
	unsigned long size;
	u32 id;

	/* ID0 */
	id = mmio_read32(gr0_base + ARM_SMMU_GR0_ID0);

	/* Only stage 2 translation is supported */
	id &= ~(ID0_S1TS | ID0_NTS);

	if (!(id & ID0_S2TS))
		return trace_error(-EIO);

	size = 1 << ((id >> ID0_NUMSIDB_SHIFT) & ID0_NUMSIDB_MASK);

	if (id & ID0_SMS) {
		size = (id >> ID0_NUMSMRG_SHIFT) & ID0_NUMSMRG_MASK;
		if (size == 0)
			return trace_error(-ENODEV);

		/* Zero-initialised to mark as invalid */
		smmu->smrs = page_alloc(&mem_pool, PAGES(size * sizeof(*smmu->smrs)));
		if (!smmu->smrs)
			return -ENOMEM;
		memset(smmu->smrs, 0, PAGES(size * sizeof(*smmu->smrs)));

		printk(" stream matching with %lu SMR groups\n", size);
	}

	smmu->cfgs = page_alloc(&mem_pool, PAGES(size * sizeof(*smmu->cfgs)));
	if (!smmu->cfgs)
		return -ENOMEM;

	smmu->num_mapping_groups = size;

	/* ID1 */
	id = mmio_read32(gr0_base + ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ID1_PAGESIZE) ? 16 : 12;

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 << (((id >> ID1_NUMPAGENDXB_SHIFT) & ID1_NUMPAGENDXB_MASK) + 1);
	size <<= smmu->pgshift;
	if (smmu->cb_base != gr0_base + size)
		printk("Warning: SMMU address space size (0x%lx) "
		       "differs from mapped region size (0x%tx)!\n",
		       size * 2, (smmu->cb_base - gr0_base) * 2);

	smmu->num_s2_context_banks = (id >> ID1_NUMS2CB_SHIFT) & ID1_NUMS2CB_MASK;
	smmu->num_context_banks = (id >> ID1_NUMCB_SHIFT) & ID1_NUMCB_MASK;
	if (smmu->num_s2_context_banks > smmu->num_context_banks)
		return trace_error(-ENODEV);

	printk(" %u context banks (%u stage-2 only)\n",
	       smmu->num_context_banks, smmu->num_s2_context_banks);

	smmu->cbs = page_alloc(&mem_pool, PAGES(smmu->num_context_banks
			       * sizeof(*smmu->cbs)));
	if (!smmu->cbs)
		return -ENOMEM;

	/* ID2 */
	id = mmio_read32(gr0_base + ARM_SMMU_GR0_ID2);
	size = arm_smmu_id_size_to_bits((id >> ID2_IAS_SHIFT) & ID2_IAS_MASK);
	smmu->ipa_size = MIN(size, get_cpu_parange());

	/* The output mask is also applied for bypass */
	size = arm_smmu_id_size_to_bits((id >> ID2_OAS_SHIFT) & ID2_OAS_MASK);
	smmu->pa_size = size;

	if (!(id & ID2_PTFS_4K))
		return trace_error(-EIO);

	printk(" stage-2: %lu-bit IPA -> %lu-bit PA\n",
	       smmu->ipa_size, smmu->pa_size);

	return 0;
}

static int arm_smmu_find_sme(u16 id, struct arm_smmu_device *smmu)
{
	struct arm_smmu_smr *smrs = smmu->smrs;
	int i, free_idx = -EINVAL;

	/* Stream indexing is blissfully easy */
	if (!smrs)
		return id;

	/* Validating SMRs is... less so */
	for (i = 0; i < smmu->num_mapping_groups; ++i) {
		if (!smrs[i].valid) {
			/*
			 * Note the first free entry we come across, which
			 * we'll claim in the end if nothing else matches.
			 */
			if (free_idx < 0)
				free_idx = i;
			continue;
		}
		/*
		 * If the new entry is _entirely_ matched by an existing entry,
		 * then reuse that, with the guarantee that there also cannot
		 * be any subsequent conflicting entries. In normal use we'd
		 * expect simply identical entries for this case, but there's
		 * no harm in accommodating the generalisation.
		 */
		if ((smmu->arm_sid_mask & smrs[i].mask) == smmu->arm_sid_mask &&
		    !((id ^ smrs[i].id) & ~smrs[i].mask)) {
			return i;
		}
		/*
		 * If the new entry has any other overlap with an existing one,
		 * though, then there always exists at least one stream ID
		 * which would cause a conflict, and we can't allow that risk.
		 */
		if (!((id ^ smrs[i].id) & ~(smrs[i].mask | smmu->arm_sid_mask)))
			return -EINVAL;
	}

	return free_idx;
}

static int arm_smmu_cell_init(struct cell *cell)
{
	struct arm_smmu_device *smmu;
	struct arm_smmu_cfg *cfg;
	struct arm_smmu_smr *smr;
	unsigned int dev, n, sid;
	int ret, idx;

	/* If no sids, ignore */
	if (!cell->config->num_stream_ids)
		return 0;

	for_each_smmu(smmu, dev) {
		cfg = &smmu->cfgs[cell->config->id];

		cfg->cbar = CBAR_TYPE_S2_TRANS;

		/*
		 * We use the cell ID here, one cell use one context, and its
		 * index is also the VMID.
		 */
		cfg->id = cell->config->id;

		ret = arm_smmu_init_context_bank(smmu, cfg, cell);
		if (ret)
			return ret;

		arm_smmu_write_context_bank(smmu, cfg->id);

		smr = smmu->smrs;

		for_each_stream_id(sid, cell->config, n) {
			ret = arm_smmu_find_sme(sid, smmu);
			if (ret < 0)
				return trace_error(ret);
			idx = ret;

			printk("Assigning StreamID 0x%x to cell \"%s\"\n",
			       sid, cell->config->name);

			arm_smmu_write_s2cr(smmu, idx, S2CR_TYPE_TRANS,
					    cfg->id);

			smr[idx].id = sid;
			smr[idx].mask = smmu->arm_sid_mask;
			smr[idx].valid = true;

			arm_smmu_write_smr(smmu, idx);
		}

		mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_TLBIVMID,
			     cfg->id);
		ret = arm_smmu_tlb_sync_global(smmu);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void arm_smmu_cell_exit(struct cell *cell)
{
	int id = cell->config->id;
	struct arm_smmu_device *smmu;
	unsigned int dev, n, sid;
	int idx;

	/* If no sids, ignore */
	if (!cell->config->num_stream_ids)
		return;

	for_each_smmu(smmu, dev) {
		for_each_stream_id(sid, cell->config, n) {
			idx = arm_smmu_find_sme(sid, smmu);
			if (idx < 0)
				continue;

			if (smmu->smrs) {
				smmu->smrs[idx].id = 0;
				smmu->smrs[idx].mask = 0;
				smmu->smrs[idx].valid = false;

				arm_smmu_write_smr(smmu, idx);
			}
			arm_smmu_write_s2cr(smmu, idx, S2CR_TYPE_FAULT, 0);

			smmu->cbs[id].cfg = NULL;
			arm_smmu_write_context_bank(smmu, id);
		}

		mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_TLBIVMID, id);
		arm_smmu_tlb_sync_global(smmu);
	}
}

static void arm_smmu_shutdown(void)
{
	struct arm_smmu_device *smmu;
	unsigned int dev;

	for_each_smmu(smmu, dev) {
		mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_sCR0,
			     sCR0_CLIENTPD);
	}
}

static int arm_smmu_init(void)
{
	struct jailhouse_iommu *iommu;
	struct arm_smmu_device *smmu;
	unsigned int n;
	int err;

	for (n = 0; n < iommu_count_units(); n++) {
		iommu = &system_config->platform_info.iommu_units[n];
		if (iommu->type != JAILHOUSE_IOMMU_ARM_MMU500)
			continue;

		smmu = &smmu_device[num_smmu_devices];
		smmu->arm_sid_mask = iommu->arm_mmu500.sid_mask;

		smmu->base = paging_map_device(iommu->base, iommu->size);
		if (!smmu->base) {
			err = -ENOMEM;
			goto error;
		}

		printk("ARM MMU500 at 0x%llx with:\n", iommu->base);

		smmu->cb_base = smmu->base + iommu->size / 2;

		err = arm_smmu_device_cfg_probe(smmu);
		if (err)
			goto error;

		err = arm_smmu_device_reset(smmu);
		if (err)
			goto error;

		num_smmu_devices++;
	}

	if (num_smmu_devices == 0)
		return 0;

	err = arm_smmu_cell_init(&root_cell);
	if (!err)
		return 0;

error:
	arm_smmu_shutdown();
	return err;
}

DEFINE_UNIT_MMIO_COUNT_REGIONS_STUB(arm_smmu);
DEFINE_UNIT(arm_smmu, "ARM SMMU");
