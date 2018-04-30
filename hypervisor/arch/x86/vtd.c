/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/ivshmem.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/unit.h>
#include <asm/apic.h>
#include <asm/iommu.h>
#include <asm/bitops.h>
#include <asm/ioapic.h>
#include <asm/spinlock.h>

#define VTD_INTERRUPT_LIMIT()	\
	system_config->platform_info.x86.vtd_interrupt_limit

#define VTD_ROOT_PRESENT		0x00000001

#define VTD_CTX_PRESENT			0x00000001
#define VTD_CTX_TTYPE_MLP_UNTRANS	0x00000000

#define VTD_CTX_AGAW_39			0x00000001
#define VTD_CTX_AGAW_48			0x00000002
#define VTD_CTX_DID_SHIFT		8

struct vtd_entry {
	u64 lo_word;
	u64 hi_word;
};

#define VTD_PAGE_READ			0x00000001
#define VTD_PAGE_WRITE			0x00000002

#define VTD_MAX_PAGE_TABLE_LEVELS	4

#define VTD_VER_REG			0x00
# define VTD_VER_MASK			BIT_MASK(7, 0)
# define VTD_VER_MIN			0x10
#define VTD_CAP_REG			0x08
# define VTD_CAP_NUM_DID_MASK		BIT_MASK(2, 0)
# define VTD_CAP_SAGAW39		(1UL << 9)
# define VTD_CAP_SAGAW48		(1UL << 10)
# define VTD_CAP_SLLPS2M		(1UL << 34)
# define VTD_CAP_SLLPS1G		(1UL << 35)
# define VTD_CAP_FRO_MASK		BIT_MASK(33, 24)
# define VTD_CAP_NFR_MASK		BIT_MASK(47, 40)
#define VTD_ECAP_REG			0x10
# define VTD_ECAP_QI			(1UL << 1)
# define VTD_ECAP_IR			(1UL << 3)
# define VTD_ECAP_EIM			(1UL << 4)
#define VTD_GCMD_REG			0x18
# define VTD_GCMD_SIRTP			(1UL << 24)
# define VTD_GCMD_IRE			(1UL << 25)
# define VTD_GCMD_QIE			(1UL << 26)
# define VTD_GCMD_SRTP			(1UL << 30)
# define VTD_GCMD_TE			(1UL << 31)
#define VTD_GSTS_REG			0x1c
# define VTD_GSTS_IRES			(1UL << 25)
# define VTD_GSTS_QIES			(1UL << 26)
# define VTD_GSTS_TES			(1UL << 31)
# define VTD_GSTS_USED_CTRLS \
	(VTD_GSTS_IRES | VTD_GSTS_QIES | VTD_GSTS_TES)
#define VTD_RTADDR_REG			0x20
#define VTD_FSTS_REG			0x34
# define VTD_FSTS_PFO			(1UL << 0)
# define VTD_FSTS_PFO_CLEAR		1
# define VTD_FSTS_PPF			(1UL << 1)
# define VTD_FSTS_FRI_MASK		BIT_MASK(15, 8)
#define VTD_FECTL_REG			0x38
#define  VTD_FECTL_IM			(1UL << 31)
#define VTD_FEDATA_REG			0x3c
#define VTD_FEADDR_REG			0x40
#define VTD_FEUADDR_REG			0x44
#define VTD_IQH_REG			0x80
# define VTD_IQH_QH_SHIFT		4
#define VTD_IQT_REG			0x88
# define VTD_IQT_QT_MASK		BIT_MASK(18, 4)
#define VTD_IQA_REG			0x90
# define VTD_IQA_ADDR_MASK		BIT_MASK(63, 12)
#define VTD_IRTA_REG			0xb8
# define VTD_IRTA_SIZE_MASK		BIT_MASK(3, 0)
# define VTD_IRTA_EIME			(1UL << 11)
# define VTD_IRTA_ADDR_MASK		BIT_MASK(63, 12)

#define VTD_REQ_INV_MASK		BIT_MASK(3, 0)

#define VTD_REQ_INV_CONTEXT		0x01
# define VTD_INV_CONTEXT_GLOBAL		(1UL << 4)
# define VTD_INV_CONTEXT_DOMAIN		(2UL << 4)
# define VTD_INV_CONTEXT_DOMAIN_SHIFT	16

#define VTD_REQ_INV_IOTLB		0x02
# define VTD_INV_IOTLB_GLOBAL		(1UL << 4)
# define VTD_INV_IOTLB_DOMAIN		(2UL << 4)
# define VTD_INV_IOTLB_DW		(1UL << 6)
# define VTD_INV_IOTLB_DR		(1UL << 7)
# define VTD_INV_IOTLB_DOMAIN_SHIFT	16

#define VTD_REQ_INV_INT			0x04
# define VTD_INV_INT_GLOBAL		(0UL << 4)
# define VTD_INV_INT_INDEX		(1UL << 4)
# define VTD_INV_INT_IM_MASK		BIT_MASK(31, 27)
# define VTD_INV_INT_IM_SHIFT		27
# define VTD_INV_INT_IIDX_MASK		BIT_MASK(47, 32)
# define VTD_INV_INT_IIDX_SHIFT		32

#define VTD_REQ_INV_WAIT		0x05
#define  VTD_INV_WAIT_IF		(1UL << 4)
#define  VTD_INV_WAIT_SW		(1UL << 5)
#define  VTD_INV_WAIT_FN		(1UL << 6)
#define  VTD_INV_WAIT_SDATA_SHIFT	32

#define VTD_FRCD_LO_REG			0x0
#define  VTD_FRCD_LO_FI_MASK		BIT_MASK(63, 12)
#define VTD_FRCD_HI_REG			0x8
#define  VTD_FRCD_HI_SID_MASK		BIT_MASK(79-64, 64-64)
#define  VTD_FRCD_HI_FR_MASK		BIT_MASK(103-64, 96-64)
#define  VTD_FRCD_HI_TYPE		(1L << (126-64))
#define  VTD_FRCD_HI_F			(1L << (127-64))
#define  VTD_FRCD_HI_F_CLEAR		1

union vtd_irte {
	struct {
		u8 p:1;
		u8 fpd:1;
		u8 dest_logical:1;
		u8 redir_hint:1;
		u8 level_triggered:1;
		u8 delivery_mode:3;
		u8 assigned:1;
		u8 reserved:7;
		u8 vector;
		u8 reserved2;
		u32 destination;
		u16 sid;
		u16 sq:2;
		u16 svt:2;
		u16 reserved3:12;
		u32 reserved4;
	} __attribute__((packed)) field;
	u64 raw[2];
} __attribute__((packed));

#define VTD_IRTE_SQ_VERIFY_FULL_SID	0x0
#define VTD_IRTE_SVT_VERIFY_SID_SQ	0x1

/* A unit can occupy up to 3 pages for registers, we reserve 4. */
#define DMAR_MMIO_SIZE			(PAGE_SIZE * 4)

struct vtd_irte_usage {
	u16 device_id;
	u16 vector:10,
	    used:1;
} __attribute__((packed));

struct vtd_emulation {
	u64 irta;
	unsigned int irt_entries;
	struct vtd_irte_usage *irte_map;

	u64 iqa;
	u16 iqh;

	u32 fault_event_regs[4];
};

static const struct vtd_entry inv_global_context = {
	.lo_word = VTD_REQ_INV_CONTEXT | VTD_INV_CONTEXT_GLOBAL,
};
static const struct vtd_entry inv_global_iotlb = {
	.lo_word = VTD_REQ_INV_IOTLB | VTD_INV_IOTLB_GLOBAL |
		VTD_INV_IOTLB_DW | VTD_INV_IOTLB_DR,
};
static const struct vtd_entry inv_global_int = {
	.lo_word = VTD_REQ_INV_INT | VTD_INV_INT_GLOBAL,
};

/* TODO: Support multiple segments */
static struct vtd_entry __attribute__((aligned(PAGE_SIZE)))
	root_entry_table[256];
static union vtd_irte *int_remap_table;
static unsigned int int_remap_table_size_log2;
static struct paging vtd_paging[VTD_MAX_PAGE_TABLE_LEVELS];
static void *dmar_reg_base;
static void *unit_inv_queue;
static unsigned int dmar_units;
static unsigned int dmar_pt_levels;
static unsigned int dmar_num_did = ~0U;
static DEFINE_SPINLOCK(inv_queue_lock);
static struct vtd_emulation root_cell_units[JAILHOUSE_MAX_IOMMU_UNITS];
static bool dmar_units_initialized;

static unsigned int vtd_mmio_count_regions(struct cell *cell)
{
	return cell == &root_cell ? iommu_count_units() : 0;
}

static unsigned int inv_queue_write(void *inv_queue, unsigned int index,
				    struct vtd_entry content)
{
	struct vtd_entry *entry = inv_queue;

	entry[index] = content;
	arch_paging_flush_cpu_caches(&entry[index], sizeof(*entry));

	return (index + 1) % (PAGE_SIZE / sizeof(*entry));
}

static void vtd_submit_iq_request(void *reg_base, void *inv_queue,
				  const struct vtd_entry *inv_request)
{
	struct vtd_entry inv_wait = {
		.lo_word = VTD_REQ_INV_WAIT | VTD_INV_WAIT_SW |
			VTD_INV_WAIT_FN | (1UL << VTD_INV_WAIT_SDATA_SHIFT),
		.hi_word = paging_hvirt2phys(
				&per_cpu(this_cpu_id())->vtd_iq_completed),
	};
	unsigned int index;

	this_cpu_data()->vtd_iq_completed = 0;

	spin_lock(&inv_queue_lock);

	index = mmio_read64_field(reg_base + VTD_IQT_REG, VTD_IQT_QT_MASK);

	if (inv_request)
		index = inv_queue_write(inv_queue, index, *inv_request);
	index = inv_queue_write(inv_queue, index, inv_wait);

	mmio_write64_field(reg_base + VTD_IQT_REG, VTD_IQT_QT_MASK, index);

	while (!this_cpu_data()->vtd_iq_completed)
		cpu_relax();

	spin_unlock(&inv_queue_lock);
}

static void vtd_flush_domain_caches(unsigned int did)
{
	const struct vtd_entry inv_context = {
		.lo_word = VTD_REQ_INV_CONTEXT | VTD_INV_CONTEXT_DOMAIN |
			(did << VTD_INV_CONTEXT_DOMAIN_SHIFT),
	};
	const struct vtd_entry inv_iotlb = {
		.lo_word = VTD_REQ_INV_IOTLB | VTD_INV_IOTLB_DOMAIN |
			VTD_INV_IOTLB_DW | VTD_INV_IOTLB_DR |
			(did << VTD_INV_IOTLB_DOMAIN_SHIFT),
	};
	void *inv_queue = unit_inv_queue;
	void *reg_base = dmar_reg_base;
	unsigned int n;

	for (n = 0; n < dmar_units; n++) {
		vtd_submit_iq_request(reg_base, inv_queue, &inv_context);
		vtd_submit_iq_request(reg_base, inv_queue, &inv_iotlb);
		reg_base += DMAR_MMIO_SIZE;
		inv_queue += PAGE_SIZE;
	}
}

static void vtd_update_gcmd_reg(void *reg_base, u32 mask, unsigned int set)
{
	u32 val = mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_USED_CTRLS;

	if (set)
		val |= mask;
	else
		val &= ~mask;
	mmio_write32(reg_base + VTD_GCMD_REG, val);

	/* Note: This test is built on the fact related bits are at the same
	 * position in VTD_GCMD_REG and VTD_GSTS_REG. */
	while ((mmio_read32(reg_base + VTD_GSTS_REG) & mask) != (val & mask))
		cpu_relax();
}

static void vtd_set_next_pt(pt_entry_t pte, unsigned long next_pt)
{
	*pte = (next_pt & BIT_MASK(51, 12)) | VTD_PAGE_READ | VTD_PAGE_WRITE;
}

static void vtd_init_fault_nmi(void)
{
	union x86_msi_vector msi = { .native.address = MSI_ADDRESS_VALUE };
	void *reg_base = dmar_reg_base;
	struct per_cpu *target_data;
	unsigned int n;

	/* Pick a suitable root cell CPU to report faults. */
	target_data = iommu_select_fault_reporting_cpu();

	/* We only support 8-bit APIC IDs. */
	msi.native.destination = (u8)target_data->public.apic_id;

	for (n = 0; n < dmar_units; n++, reg_base += DMAR_MMIO_SIZE) {
		/* Mask events */
		mmio_write32_field(reg_base + VTD_FECTL_REG, VTD_FECTL_IM, 1);

		/*
		 * VT-d spec rev. 2.3 section 7.4 suggests that only reading
		 * back FSTS or FECTL ensures no interrupt messages are still
		 * in-flight when we change their destination below.
		 */
		mmio_read32(reg_base + VTD_FECTL_REG);

		/* Program MSI message to send NMIs to the target CPU */
		mmio_write32(reg_base + VTD_FEDATA_REG, MSI_DM_NMI);
		mmio_write32(reg_base + VTD_FEADDR_REG, (u32)msi.raw.address);
		mmio_write32(reg_base + VTD_FEUADDR_REG, 0);

		/* Unmask events */
		mmio_write32_field(reg_base + VTD_FECTL_REG, VTD_FECTL_IM, 0);
	}

	/*
	 * There is a race window between setting the new reporting CPU ID and
	 * updating the target programming in the register. If a fault hits us
	 * in this window and no other NMIs arrive after that, the event will
	 * not be reported. Address this by triggering an NMI on the new
	 * reporting CPU.
	 */
	apic_send_nmi_ipi(&target_data->public);
}

static void *vtd_get_fault_rec_reg_addr(void *reg_base)
{
	return reg_base + 16 *
		mmio_read64_field(reg_base + VTD_CAP_REG, VTD_CAP_FRO_MASK);
}

static void vtd_print_fault_record_reg_status(unsigned int unit_no,
					      void *reg_base)
{
	unsigned int sid = mmio_read64_field(reg_base + VTD_FRCD_HI_REG,
					     VTD_FRCD_HI_SID_MASK);
	unsigned int fr = mmio_read64_field(reg_base + VTD_FRCD_HI_REG,
					    VTD_FRCD_HI_FR_MASK);
	unsigned long fi = mmio_read64_field(reg_base + VTD_FRCD_LO_REG,
					     VTD_FRCD_LO_FI_MASK);
	unsigned int type = mmio_read64_field(reg_base + VTD_FRCD_HI_REG,
					      VTD_FRCD_HI_TYPE);

	printk("VT-d fault event reported by IOMMU %d:\n", unit_no);
	printk(" Source Identifier (bus:dev.func): %02x:%02x.%x\n",
	       PCI_BDF_PARAMS(sid));
	printk(" Fault Reason: 0x%x Fault Info: %lx Type %d\n", fr, fi, type);
}

void iommu_check_pending_faults(void)
{
	unsigned int fr_index;
	void *reg_base = dmar_reg_base;
	unsigned int n;
	void *fault_reg_addr, *rec_reg_addr;

	if (this_cpu_id() != fault_reporting_cpu_id)
		return;

	for (n = 0; n < dmar_units; n++, reg_base += DMAR_MMIO_SIZE)
		if (mmio_read32_field(reg_base + VTD_FSTS_REG, VTD_FSTS_PPF)) {
			fr_index = mmio_read32_field(reg_base + VTD_FSTS_REG,
						     VTD_FSTS_FRI_MASK);
			fault_reg_addr = vtd_get_fault_rec_reg_addr(reg_base);
			rec_reg_addr = fault_reg_addr + 16 * fr_index;
			vtd_print_fault_record_reg_status(n, rec_reg_addr);

			/* Clear faults in record registers */
			mmio_write64_field(rec_reg_addr + VTD_FRCD_HI_REG,
					   VTD_FRCD_HI_F, VTD_FRCD_HI_F_CLEAR);
		}
}

static int vtd_emulate_inv_int(unsigned int unit_no, unsigned int index)
{
	struct vtd_irte_usage *irte_usage;
	struct apic_irq_message irq_msg;
	struct pci_device *device;

	if (index >= root_cell_units[unit_no].irt_entries)
		return 0;
	irte_usage = &root_cell_units[unit_no].irte_map[index];
	if (!irte_usage->used)
		return 0;

	device = pci_get_assigned_device(&root_cell, irte_usage->device_id);
	if (device && device->info->type == JAILHOUSE_PCI_TYPE_IVSHMEM)
		return arch_ivshmem_update_msix(device);

	irq_msg = iommu_get_remapped_root_int(unit_no, irte_usage->device_id,
					      irte_usage->vector, index);
	return iommu_map_interrupt(&root_cell, irte_usage->device_id,
				   irte_usage->vector, irq_msg);
}

static int vtd_emulate_qi_request(unsigned int unit_no,
				  struct vtd_entry inv_desc)
{
	unsigned int start, count, n;
	void *status_page;
	int result;

	switch (inv_desc.lo_word & VTD_REQ_INV_MASK) {
	case VTD_REQ_INV_INT:
		if (inv_desc.lo_word & VTD_INV_INT_INDEX) {
			start = (inv_desc.lo_word & VTD_INV_INT_IIDX_MASK) >>
				VTD_INV_INT_IIDX_SHIFT;
			count =
			    1 << ((inv_desc.lo_word & VTD_INV_INT_IM_MASK) >>
				  VTD_INV_INT_IM_SHIFT);
		} else {
			start = 0;
			count = root_cell_units[unit_no].irt_entries;
		}
		for (n = start; n < start + count; n++) {
			result = vtd_emulate_inv_int(unit_no, n);
			if (result < 0)
				return result;
		}
		return 0;
	case VTD_REQ_INV_WAIT:
		if (inv_desc.lo_word & VTD_INV_WAIT_IF ||
		    !(inv_desc.lo_word & VTD_INV_WAIT_SW))
			return -EINVAL;

		status_page = paging_get_guest_pages(NULL, inv_desc.hi_word, 1,
						     PAGE_DEFAULT_FLAGS);
		if (!status_page)
			return -EINVAL;

		*(u32 *)(status_page + (inv_desc.hi_word & ~PAGE_MASK)) =
			inv_desc.lo_word >> 32;

		return 0;
	}
	return -EINVAL;
}

static enum mmio_result vtd_unit_access_handler(void *arg,
						struct mmio_access *mmio)
{
	struct vtd_emulation *unit = arg;
	unsigned int unit_no = unit - root_cell_units;
	struct vtd_entry inv_desc;
	void *inv_desc_page;
	unsigned int reg;

	if (mmio->address == VTD_FSTS_REG && !mmio->is_write) {
		/*
		 * Nothing to report this way, vtd_check_pending_faults takes
		 * care for the whole system.
		 */
		mmio->value = 0;
		return MMIO_HANDLED;
	}
	if (mmio->address >= VTD_FECTL_REG &&
	    mmio->address <= VTD_FEUADDR_REG) {
		/*
		 * Direct the access to the shadow registers - we handle the
		 * real fault events now.
		 */
		reg = (mmio->address - VTD_FECTL_REG) / 4;
		if (mmio->is_write)
			unit->fault_event_regs[reg] = mmio->value;
		else
			mmio->value = unit->fault_event_regs[reg];
		return MMIO_HANDLED;
	}
	if (mmio->address == VTD_IQT_REG && mmio->is_write) {
		while (unit->iqh != (mmio->value & ~PAGE_MASK)) {
			inv_desc_page =
				paging_get_guest_pages(NULL, unit->iqa, 1,
						       PAGE_READONLY_FLAGS);
			if (!inv_desc_page)
				goto invalid_iq_entry;

			inv_desc =
			    *(struct vtd_entry *)(inv_desc_page + unit->iqh);

			if (vtd_emulate_qi_request(unit_no, inv_desc) != 0)
				goto invalid_iq_entry;

			unit->iqh += 1 << VTD_IQH_QH_SHIFT;
			unit->iqh &= ~PAGE_MASK;
		}
		return MMIO_HANDLED;
	}
	panic_printk("FATAL: Unhandled DMAR unit %s access, register %02lx\n",
		     mmio->is_write ? "write" : "read", mmio->address);
	return MMIO_ERROR;

invalid_iq_entry:
	panic_printk("FATAL: Invalid/unsupported invalidation queue entry\n");
	return -1;
}

static void vtd_init_unit(void *reg_base, void *inv_queue)
{
	void *fault_reg_base;
	unsigned int nfr, n;

	/* Disabled QI and IR in case it was already on */
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_QIE, 0);
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_IRE, 0);

	nfr = mmio_read64_field(reg_base + VTD_CAP_REG, VTD_CAP_NFR_MASK);
	fault_reg_base = vtd_get_fault_rec_reg_addr(reg_base);

	for (n = 0; n < nfr; n++)
		/* Clear fault recording register status */
		mmio_write64_field(fault_reg_base + 16 * n + VTD_FRCD_HI_REG,
				   VTD_FRCD_HI_F, VTD_FRCD_HI_F_CLEAR);

	/* Clear fault overflow status */
	mmio_write32_field(reg_base + VTD_FSTS_REG, VTD_FSTS_PFO,
			   VTD_FSTS_PFO_CLEAR);

	/* Set root entry table pointer */
	mmio_write64(reg_base + VTD_RTADDR_REG,
		     paging_hvirt2phys(root_entry_table));
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_SRTP, 1);

	/* Set interrupt remapping table pointer */
	mmio_write64(reg_base + VTD_IRTA_REG,
		     paging_hvirt2phys(int_remap_table) |
		     (using_x2apic ? VTD_IRTA_EIME : 0) |
		     (int_remap_table_size_log2 - 1));
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_SIRTP, 1);

	/* Setup and activate invalidation queue */
	mmio_write64(reg_base + VTD_IQT_REG, 0);
	mmio_write64(reg_base + VTD_IQA_REG, paging_hvirt2phys(inv_queue));
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_QIE, 1);

	vtd_submit_iq_request(reg_base, inv_queue, &inv_global_context);
	vtd_submit_iq_request(reg_base, inv_queue, &inv_global_iotlb);
	vtd_submit_iq_request(reg_base, inv_queue, &inv_global_int);

	vtd_update_gcmd_reg(reg_base, VTD_GCMD_TE, 1);
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_IRE, 1);
}

static void vtd_update_irte(unsigned int index, union vtd_irte content)
{
	const struct vtd_entry inv_int = {
		.lo_word = VTD_REQ_INV_INT | VTD_INV_INT_INDEX |
			((u64)index << VTD_INV_INT_IIDX_SHIFT),
	};
	union vtd_irte *irte = &int_remap_table[index];
	void *inv_queue = unit_inv_queue;
	void *reg_base = dmar_reg_base;
	unsigned int n;

	if (content.field.p) {
		/*
		 * Write upper half first to preserve non-presence.
		 * If the entry was present before, we are only modifying the
		 * lower half's content (destination etc.), so writing the
		 * upper half becomes a nop and is safely done first.
		 */
		irte->raw[1] = content.raw[1];
		memory_barrier();
		irte->raw[0] = content.raw[0];
	} else {
		/*
		 * Write only lower half - we are clearing presence and
		 * assignment.
		 */
		irte->raw[0] = content.raw[0];
	}
	arch_paging_flush_cpu_caches(irte, sizeof(*irte));

	for (n = 0; n < dmar_units; n++) {
		vtd_submit_iq_request(reg_base, inv_queue, &inv_int);
		reg_base += DMAR_MMIO_SIZE;
		inv_queue += PAGE_SIZE;
	}
}

static int vtd_find_int_remap_region(u16 device_id)
{
	int n;

	/* VTD_INTERRUPT_LIMIT() is < 2^16, see vtd_init */
	for (n = 0; n < VTD_INTERRUPT_LIMIT(); n++)
		if (int_remap_table[n].field.assigned &&
		    int_remap_table[n].field.sid == device_id)
			return n;

	return -ENOENT;
}

static int vtd_reserve_int_remap_region(u16 device_id, unsigned int length)
{
	int n, start = -E2BIG;

	if (length == 0 || vtd_find_int_remap_region(device_id) >= 0)
		return 0;

	for (n = 0; n < VTD_INTERRUPT_LIMIT(); n++) {
		if (int_remap_table[n].field.assigned) {
			start = -E2BIG;
			continue;
		}
		if (start < 0)
			start = n;
		if (n + 1 == start + length) {
			printk("Reserving %u interrupt(s) for device %04x "
			       "at index %d\n", length, device_id, start);
			for (n = start; n < start + length; n++) {
				int_remap_table[n].field.assigned = 1;
				int_remap_table[n].field.sid = device_id;
			}
			return start;
		}
	}
	return trace_error(-E2BIG);
}

static void vtd_free_int_remap_region(u16 device_id, unsigned int length)
{
	union vtd_irte free_irte = { .field.p = 0, .field.assigned = 0 };
	int pos = vtd_find_int_remap_region(device_id);

	if (pos >= 0) {
		printk("Freeing %u interrupt(s) for device %04x at index %d\n",
		       length, device_id, pos);
		while (length-- > 0)
			vtd_update_irte(pos++, free_irte);
	}
}

int iommu_add_pci_device(struct cell *cell, struct pci_device *device)
{
	unsigned int max_vectors = MAX(device->info->num_msi_vectors,
				       device->info->num_msix_vectors);
	u16 bdf = device->info->bdf;
	u64 *root_entry_lo = &root_entry_table[PCI_BUS(bdf)].lo_word;
	struct vtd_entry *context_entry_table, *context_entry;
	int result;

	result = vtd_reserve_int_remap_region(bdf, max_vectors);
	if (result < 0)
		return result;

	if (*root_entry_lo & VTD_ROOT_PRESENT) {
		context_entry_table =
			paging_phys2hvirt(*root_entry_lo & PAGE_MASK);
	} else {
		context_entry_table = page_alloc(&mem_pool, 1);
		if (!context_entry_table)
			goto error_nomem;
		*root_entry_lo = VTD_ROOT_PRESENT |
			paging_hvirt2phys(context_entry_table);
		arch_paging_flush_cpu_caches(root_entry_lo, sizeof(u64));
	}

	context_entry = &context_entry_table[PCI_DEVFN(bdf)];
	context_entry->lo_word = VTD_CTX_PRESENT | VTD_CTX_TTYPE_MLP_UNTRANS |
		paging_hvirt2phys(cell->arch.vtd.pg_structs.root_table);
	context_entry->hi_word =
		(dmar_pt_levels == 3 ? VTD_CTX_AGAW_39 : VTD_CTX_AGAW_48) |
		(cell->config->id << VTD_CTX_DID_SHIFT);
	arch_paging_flush_cpu_caches(context_entry, sizeof(*context_entry));

	return 0;

error_nomem:
	vtd_free_int_remap_region(bdf, max_vectors);
	return -ENOMEM;
}

void iommu_remove_pci_device(struct pci_device *device)
{
	u16 bdf = device->info->bdf;
	u64 *root_entry_lo = &root_entry_table[PCI_BUS(bdf)].lo_word;
	struct vtd_entry *context_entry_table;
	struct vtd_entry *context_entry;
	unsigned int n;

	vtd_free_int_remap_region(bdf, MAX(device->info->num_msi_vectors,
					   device->info->num_msix_vectors));

	context_entry_table = paging_phys2hvirt(*root_entry_lo & PAGE_MASK);
	context_entry = &context_entry_table[PCI_DEVFN(bdf)];

	context_entry->lo_word &= ~VTD_CTX_PRESENT;
	arch_paging_flush_cpu_caches(&context_entry->lo_word, sizeof(u64));

	for (n = 0; n < 256; n++)
		if (context_entry_table[n].lo_word & VTD_CTX_PRESENT)
			return;

	*root_entry_lo &= ~VTD_ROOT_PRESENT;
	arch_paging_flush_cpu_caches(root_entry_lo, sizeof(u64));
	page_free(&mem_pool, context_entry_table, 1);
}

static void vtd_cell_exit(struct cell *cell);

static int vtd_cell_init(struct cell *cell)
{
	const struct jailhouse_irqchip *irqchip =
		jailhouse_cell_irqchips(cell->config);
	struct phys_ioapic *ioapic;
	unsigned int n;
	int result;

	if (cell->config->id >= dmar_num_did)
		return trace_error(-ERANGE);

	cell->arch.vtd.pg_structs.root_paging = vtd_paging;
	cell->arch.vtd.pg_structs.root_table = page_alloc(&mem_pool, 1);
	if (!cell->arch.vtd.pg_structs.root_table)
		return -ENOMEM;

	/* reserve regions for IRQ chips (if not done already) */
	for (n = 0; n < cell->config->num_irqchips; n++, irqchip++) {
		result = ioapic_get_or_add_phys(irqchip, &ioapic);
		if (result == 0)
			result = vtd_reserve_int_remap_region(irqchip->id,
							      ioapic->pins);
		if (result < 0) {
			vtd_cell_exit(cell);
			return result;
		}
	}

	return 0;
}

int iommu_map_memory_region(struct cell *cell,
			    const struct jailhouse_memory *mem)
{
	u32 flags = 0;

	if (!(mem->flags & JAILHOUSE_MEM_DMA))
		return 0;

	if (mem->virt_start & BIT_MASK(63, 12 + 9 * dmar_pt_levels))
		return trace_error(-E2BIG);

	if (mem->flags & JAILHOUSE_MEM_READ)
		flags |= VTD_PAGE_READ;
	if (mem->flags & JAILHOUSE_MEM_WRITE)
		flags |= VTD_PAGE_WRITE;

	return paging_create(&cell->arch.vtd.pg_structs, mem->phys_start,
			     mem->size, mem->virt_start, flags,
			     PAGING_COHERENT);
}

int iommu_unmap_memory_region(struct cell *cell,
			      const struct jailhouse_memory *mem)
{
	if (!(mem->flags & JAILHOUSE_MEM_DMA))
		return 0;

	return paging_destroy(&cell->arch.vtd.pg_structs, mem->virt_start,
			      mem->size, PAGING_COHERENT);
}

struct apic_irq_message
iommu_get_remapped_root_int(unsigned int iommu, u16 device_id,
			    unsigned int vector, unsigned int remap_index)
{
	struct vtd_emulation *unit = &root_cell_units[iommu];
	struct apic_irq_message irq_msg = { .valid = 0 };
	union vtd_irte root_irte;
	unsigned long irte_addr;
	void *irte_page;

	if (remap_index >= unit->irt_entries)
		return irq_msg;
	unit->irte_map[remap_index].used = 0;

	irte_addr = (unit->irta & VTD_IRTA_ADDR_MASK) +
		remap_index * sizeof(union vtd_irte);
	irte_page = paging_get_guest_pages(NULL, irte_addr, 1,
					   PAGE_READONLY_FLAGS);
	if (!irte_page)
		return irq_msg;

	root_irte = *(union vtd_irte *)(irte_page + (irte_addr & ~PAGE_MASK));

	irq_msg.valid =
		(root_irte.field.p && root_irte.field.sid == device_id);
	irq_msg.vector = root_irte.field.vector;
	irq_msg.delivery_mode = root_irte.field.delivery_mode;
	irq_msg.dest_logical = root_irte.field.dest_logical;
	irq_msg.level_triggered = root_irte.field.level_triggered;
	irq_msg.redir_hint = root_irte.field.redir_hint;
	irq_msg.destination = root_irte.field.destination;
	if (!using_x2apic)
		/* xAPIC in flat mode: APIC ID in 47:40 (of 63:32) */
		irq_msg.destination >>= 8;

	unit->irte_map[remap_index].device_id = device_id;
	unit->irte_map[remap_index].vector = vector;
	unit->irte_map[remap_index].used = 1;

	return irq_msg;
}

int iommu_map_interrupt(struct cell *cell, u16 device_id, unsigned int vector,
			struct apic_irq_message irq_msg)
{
	union vtd_irte irte;
	int base_index;

	base_index = vtd_find_int_remap_region(device_id);
	if (base_index < 0)
		return base_index;

	if (vector >= VTD_INTERRUPT_LIMIT() ||
	    base_index >= VTD_INTERRUPT_LIMIT() - vector)
		return -ERANGE;

	irte = int_remap_table[base_index + vector];
	if (!irte.field.assigned || irte.field.sid != device_id)
		return -ERANGE;

	irte.field.p = irq_msg.valid;
	if (!irte.field.p)
		/*
		 * Do not validate non-present entries, they may contain
		 * invalid data and cause false-positives.
		 */
		goto update_irte;

	/*
	 * If redirection hint is cleared, physical destination mode is used
	 * effectively (destination mode bit is ignored, only a single CPU is
	 * targeted). Fix up irq_msg so that apic_filter_irq_dest uses the
	 * appropriate mode.
	 */
	irq_msg.dest_logical = irq_msg.dest_logical && irq_msg.redir_hint;

	/* Validate delivery mode and destination(s). */
	if (irq_msg.delivery_mode != APIC_MSG_DLVR_FIXED &&
	    irq_msg.delivery_mode != APIC_MSG_DLVR_LOWPRI)
		return -EINVAL;
	if (!apic_filter_irq_dest(cell, &irq_msg))
		return -EPERM;

	irte.field.dest_logical = irq_msg.dest_logical;
	irte.field.redir_hint = irq_msg.redir_hint;
	irte.field.level_triggered = irq_msg.level_triggered;
	irte.field.delivery_mode = irq_msg.delivery_mode;
	irte.field.vector = irq_msg.vector;
	irte.field.destination = irq_msg.destination;
	if (!using_x2apic)
		/* xAPIC in flat mode: APIC ID in 47:40 (of 63:32) */
		irte.field.destination <<= 8;
	irte.field.sq = VTD_IRTE_SQ_VERIFY_FULL_SID;
	irte.field.svt = VTD_IRTE_SVT_VERIFY_SID_SQ;

update_irte:
	vtd_update_irte(base_index + vector, irte);

	return base_index + vector;
}

static void vtd_cell_exit(struct cell *cell)
{
	page_free(&mem_pool, cell->arch.vtd.pg_structs.root_table, 1);

	/*
	 * Note that reservation regions of IOAPICs won't be released because
	 * they might be shared with other cells
	 */
}

void iommu_config_commit(struct cell *cell_added_removed)
{
	void *inv_queue = unit_inv_queue;
	void *reg_base = dmar_reg_base;
	int n;

	if (cell_added_removed)
		vtd_init_fault_nmi();

	if (cell_added_removed == &root_cell) {
		for (n = 0; n < dmar_units; n++) {
			vtd_init_unit(reg_base, inv_queue);
			reg_base += DMAR_MMIO_SIZE;
			inv_queue += PAGE_SIZE;
		}
		dmar_units_initialized = true;
	} else {
		if (cell_added_removed)
			vtd_flush_domain_caches(cell_added_removed->config->id);
		vtd_flush_domain_caches(root_cell.config->id);
	}
}

static void vtd_restore_ir(unsigned int unit_no, void *reg_base)
{
	struct vtd_emulation *unit = &root_cell_units[unit_no];
	void *inv_queue = unit_inv_queue + unit_no * PAGE_SIZE;
	void *root_inv_queue;
	u64 iqh;
	int n;

	mmio_write64(reg_base + VTD_IRTA_REG, unit->irta);
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_SIRTP, 1);
	vtd_submit_iq_request(reg_base, inv_queue, &inv_global_int);

	vtd_update_gcmd_reg(reg_base, VTD_GCMD_QIE, 0);
	mmio_write64(reg_base + VTD_IQT_REG, 0);
	mmio_write64(reg_base + VTD_IQA_REG, unit->iqa);
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_QIE, 1);

	/*
	 * Restore invalidation queue head pointer by issuing dummy requests
	 * until the hardware is in sync with the Linux state again.
	 */
	iqh =unit->iqh;
	root_inv_queue = paging_get_guest_pages(NULL, unit->iqa, 1,
						PAGE_DEFAULT_FLAGS);
	if (root_inv_queue)
		while (mmio_read64(reg_base + VTD_IQH_REG) != iqh)
			vtd_submit_iq_request(reg_base, root_inv_queue, NULL);
	else
		printk("WARNING: Failed to restore invalidation queue head\n");

	vtd_update_gcmd_reg(reg_base, VTD_GCMD_IRE, 1);

	/* Mask events */
	mmio_write32_field(reg_base + VTD_FECTL_REG, VTD_FECTL_IM, 1);

	/* Restore, unmasking with the last write. */
	for (n = ARRAY_SIZE(unit->fault_event_regs) - 1; n >= 0; n--)
		mmio_write32(reg_base + VTD_FECTL_REG + n * 4,
			     unit->fault_event_regs[n]);
}

static int vtd_init_ir_emulation(unsigned int unit_no, void *reg_base)
{
	struct vtd_emulation *unit = &root_cell_units[unit_no];
	unsigned long base, size;
	unsigned int n;
	u64 iqt;

	root_cell.arch.vtd.ir_emulation = true;

	base = system_config->platform_info.x86.iommu_units[unit_no].base;
	mmio_region_register(&root_cell, base, PAGE_SIZE,
			     vtd_unit_access_handler, unit);

	unit->irta = mmio_read64(reg_base + VTD_IRTA_REG);
	unit->irt_entries = 2 << (unit->irta & VTD_IRTA_SIZE_MASK);

	size = PAGE_ALIGN(sizeof(struct vtd_irte_usage) * unit->irt_entries);
	unit->irte_map = page_alloc(&mem_pool, size / PAGE_SIZE);
	if (!unit->irte_map)
		return -ENOMEM;

	iqt = mmio_read64(reg_base + VTD_IQT_REG);
	while (mmio_read64(reg_base + VTD_IQH_REG) != iqt)
		cpu_relax();
	unit->iqh = iqt;

	unit->iqa = mmio_read64(reg_base + VTD_IQA_REG);
	if (unit->iqa & ~VTD_IQA_ADDR_MASK)
		return trace_error(-EIO);

	for (n = 0; n < ARRAY_SIZE(unit->fault_event_regs); n++)
		unit->fault_event_regs[n] =
			mmio_read32(reg_base + VTD_FECTL_REG + n * 4);

	return 0;
}

static int vtd_init(void)
{
	unsigned long version, caps, ecaps, ctrls, sllps_caps = ~0UL;
	unsigned int units, pt_levels, num_did, n;
	struct jailhouse_iommu *unit;
	void *reg_base;
	int err;

	/* n = roundup(log2(VTD_INTERRUPT_LIMIT())) */
	for (n = 0; (1UL << n) < VTD_INTERRUPT_LIMIT(); n++)
		; /* empty loop */
	if (n >= 16)
		return trace_error(-EINVAL);

	int_remap_table =
		page_alloc(&mem_pool, PAGES(sizeof(union vtd_irte) << n));
	if (!int_remap_table)
		return -ENOMEM;

	int_remap_table_size_log2 = n;

	units = iommu_count_units();
	if (units == 0)
		return trace_error(-EINVAL);

	dmar_reg_base = page_alloc(&remap_pool, units * PAGES(DMAR_MMIO_SIZE));
	if (!dmar_reg_base)
		return trace_error(-ENOMEM);

	unit_inv_queue = page_alloc(&mem_pool, units);
	if (!unit_inv_queue)
		return -ENOMEM;

	for (n = 0; n < units; n++) {
		unit = &system_config->platform_info.x86.iommu_units[n];

		reg_base = dmar_reg_base + n * DMAR_MMIO_SIZE;

		err = paging_create(&hv_paging_structs, unit->base, unit->size,
				    (unsigned long)reg_base,
				    PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE,
				    PAGING_NON_COHERENT);
		if (err)
			return err;

		version = mmio_read64(reg_base + VTD_VER_REG) & VTD_VER_MASK;
		if (version < VTD_VER_MIN || version == 0xff)
			return trace_error(-EIO);

		printk("DMAR unit @0x%llx/0x%x\n", unit->base, unit->size);

		caps = mmio_read64(reg_base + VTD_CAP_REG);
		if (caps & VTD_CAP_SAGAW39)
			pt_levels = 3;
		else if (caps & VTD_CAP_SAGAW48)
			pt_levels = 4;
		else
			return trace_error(-EIO);
		sllps_caps &= caps;

		if (dmar_pt_levels > 0 && dmar_pt_levels != pt_levels)
			return trace_error(-EIO);
		dmar_pt_levels = pt_levels;

		ecaps = mmio_read64(reg_base + VTD_ECAP_REG);
		if (!(ecaps & VTD_ECAP_QI) || !(ecaps & VTD_ECAP_IR) ||
		    (using_x2apic && !(ecaps & VTD_ECAP_EIM)))
			return trace_error(-EIO);

		ctrls = mmio_read32(reg_base + VTD_GSTS_REG) &
			VTD_GSTS_USED_CTRLS;
		if (ctrls != 0) {
			if (ctrls != (VTD_GSTS_IRES | VTD_GSTS_QIES))
				return trace_error(-EBUSY);
			err = vtd_init_ir_emulation(n, reg_base);
			if (err)
				return err;
		} else if (root_cell.arch.vtd.ir_emulation) {
			/* IR+QI must be either on or off in all units */
			return trace_error(-EIO);
		}

		num_did = 1 << (4 + (caps & VTD_CAP_NUM_DID_MASK) * 2);
		if (num_did < dmar_num_did)
			dmar_num_did = num_did;
	}

	dmar_units = units;

	/*
	 * Derive vdt_paging from very similar x86_64_paging,
	 * replicating 0..3 for 4 levels and 1..3 for 3 levels.
	 */
	memcpy(vtd_paging, &x86_64_paging[4 - dmar_pt_levels],
	       sizeof(struct paging) * dmar_pt_levels);
	for (n = 0; n < dmar_pt_levels; n++)
		vtd_paging[n].set_next_pt = vtd_set_next_pt;
	if (!(sllps_caps & VTD_CAP_SLLPS1G))
		vtd_paging[dmar_pt_levels - 3].page_size = 0;
	if (!(sllps_caps & VTD_CAP_SLLPS2M))
		vtd_paging[dmar_pt_levels - 2].page_size = 0;

	return vtd_cell_init(&root_cell);
}

void iommu_prepare_shutdown(void)
{
	void *reg_base = dmar_reg_base;
	unsigned int n;

	if (dmar_units_initialized)
		for (n = 0; n < dmar_units; n++, reg_base += DMAR_MMIO_SIZE) {
			vtd_update_gcmd_reg(reg_base, VTD_GCMD_TE, 0);
			vtd_update_gcmd_reg(reg_base, VTD_GCMD_IRE, 0);
			if (root_cell.arch.vtd.ir_emulation)
				vtd_restore_ir(n, reg_base);
			else
				vtd_update_gcmd_reg(reg_base, VTD_GCMD_QIE, 0);
		}
}

bool iommu_cell_emulates_ir(struct cell *cell)
{
	return cell->arch.vtd.ir_emulation;
}

DEFINE_UNIT_SHUTDOWN_STUB(vtd);
DEFINE_UNIT(vtd, "VT-d");
