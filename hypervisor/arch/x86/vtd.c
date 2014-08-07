/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <asm/bitops.h>
#include <asm/ioapic.h>
#include <asm/spinlock.h>
#include <asm/vtd.h>

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
static struct paging vtd_paging[VTD_MAX_PAGE_DIR_LEVELS];
static void *dmar_reg_base;
static void *unit_inv_queue;
static unsigned int dmar_units;
static unsigned int dmar_pt_levels;
static unsigned int dmar_num_did = ~0U;
static unsigned int fault_reporting_cpu_id;
static DEFINE_SPINLOCK(inv_queue_lock);

static unsigned int inv_queue_write(void *inv_queue, unsigned int index,
				    struct vtd_entry content)
{
	struct vtd_entry *entry = inv_queue;

	entry[index] = content;
	flush_cache(&entry[index], sizeof(*entry));

	return (index + 1) % (PAGE_SIZE / sizeof(*entry));
}

static void vtd_submit_iq_request(void *reg_base, void *inv_queue,
				  const struct vtd_entry *inv_request)
{
	volatile u32 completed = 0;
	struct vtd_entry inv_wait = {
		.lo_word = VTD_REQ_INV_WAIT | VTD_INV_WAIT_SW |
			VTD_INV_WAIT_FN | (1UL << VTD_INV_WAIT_SDATA_SHIFT),
		.hi_word = page_map_hvirt2phys(&completed),
	};
	unsigned int index;

	spin_lock(&inv_queue_lock);

	index = mmio_read64_field(reg_base + VTD_IQT_REG, VTD_IQT_QT_MASK);

	index = inv_queue_write(inv_queue, index, *inv_request);
	index = inv_queue_write(inv_queue, index, inv_wait);

	mmio_write64_field(reg_base + VTD_IQT_REG, VTD_IQT_QT_MASK, index);

	while (!completed)
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
		reg_base += PAGE_SIZE;
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
	*pte = (next_pt & 0x000ffffffffff000UL) | VTD_PAGE_READ |
		VTD_PAGE_WRITE;
}

static void vtd_init_fault_nmi(void)
{
	union x86_msi_vector msi = { .native.address = MSI_ADDRESS_VALUE };
	void *reg_base = dmar_reg_base;
	struct per_cpu *cpu_data;
	unsigned int n;

	/* Assume that at least one bit is set somewhere as
	 * we don't support configurations when Linux is left with no CPUs */
	for (n = 0; root_cell.cpu_set->bitmap[n] == 0; n++)
		/* Empty loop */;
	cpu_data = per_cpu(ffsl(root_cell.cpu_set->bitmap[n]));

	/* We only support 8-bit APIC IDs. */
	msi.native.destination = (u8)cpu_data->apic_id;

	/* Save this value globally to avoid multiple reports of the same
	 * case from different CPUs */
	fault_reporting_cpu_id = cpu_data->cpu_id;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE) {
		/* Mask events */
		mmio_write32_field(reg_base + VTD_FECTL_REG, VTD_FECTL_IM, 1);

		/* Program MSI message to send NMIs to the target CPU */
		mmio_write32(reg_base + VTD_FEDATA_REG, MSI_DM_NMI);
		mmio_write32(reg_base + VTD_FEADDR_REG, (u32)msi.raw.address);
		mmio_write32(reg_base + VTD_FEUADDR_REG, 0);

		/* Unmask events */
		mmio_write32_field(reg_base + VTD_FECTL_REG, VTD_FECTL_IM, 0);
	}
}

static void *vtd_get_fault_rec_reg_addr(void *reg_base)
{
	return reg_base + 16 *
		mmio_read64_field(reg_base + VTD_CAP_REG, VTD_CAP_FRO_MASK);
}

static void vtd_print_fault_record_reg_status(void *reg_base)
{
	unsigned int sid = mmio_read64_field(reg_base + VTD_FRCD_HI_REG,
					     VTD_FRCD_HI_SID_MASK);
	unsigned int fr = mmio_read64_field(reg_base + VTD_FRCD_HI_REG,
					    VTD_FRCD_HI_FR_MASK);
	unsigned long fi = mmio_read64_field(reg_base + VTD_FRCD_LO_REG,
					     VTD_FRCD_LO_FI_MASK);
	unsigned int type = mmio_read64_field(reg_base + VTD_FRCD_HI_REG,
					      VTD_FRCD_HI_TYPE);

	printk("VT-d fault event occurred:\n");
	printk(" Source Identifier (bus:dev.func): %02x:%02x.%x\n",
	       PCI_BDF_PARAMS(sid));
	printk(" Fault Reason: 0x%x Fault Info: %lx Type %d\n", fr, fi, type);
}

void vtd_check_pending_faults(struct per_cpu *cpu_data)
{
	unsigned int fr_index;
	void *reg_base = dmar_reg_base;
	unsigned int n;
	void *fault_reg_addr, *rec_reg_addr;

	if (cpu_data->cpu_id != fault_reporting_cpu_id)
		return;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE)
		if (mmio_read32_field(reg_base + VTD_FSTS_REG, VTD_FSTS_PPF)) {
			fr_index = mmio_read32_field(reg_base + VTD_FSTS_REG,
						     VTD_FSTS_FRI_MASK);
			fault_reg_addr = vtd_get_fault_rec_reg_addr(reg_base);
			rec_reg_addr = fault_reg_addr + 16 * fr_index;
			vtd_print_fault_record_reg_status(rec_reg_addr);

			/* Clear faults in record registers */
			mmio_write64_field(rec_reg_addr + VTD_FRCD_HI_REG,
					   VTD_FRCD_HI_F, VTD_FRCD_HI_F_CLEAR);
		}
}

static void vtd_init_unit(void *reg_base, void *inv_queue)
{
	void *fault_reg_base;
	unsigned int nfr, n;

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
		     page_map_hvirt2phys(root_entry_table));
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_SRTP, 1);

	/* Set interrupt remapping table pointer */
	mmio_write64(reg_base + VTD_IRTA_REG,
		     page_map_hvirt2phys(int_remap_table) |
		     (int_remap_table_size_log2 - 1));
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_SIRTP, 1);

	/* Setup and activate invalidation queue */
	mmio_write64(reg_base + VTD_IQT_REG, 0);
	mmio_write64(reg_base + VTD_IQA_REG, page_map_hvirt2phys(inv_queue));
	vtd_update_gcmd_reg(reg_base, VTD_GCMD_QIE, 1);

	vtd_submit_iq_request(reg_base, inv_queue, &inv_global_context);
	vtd_submit_iq_request(reg_base, inv_queue, &inv_global_iotlb);
	vtd_submit_iq_request(reg_base, inv_queue, &inv_global_int);
}

int vtd_init(void)
{
	unsigned long size, caps, ecaps, sllps_caps = ~0UL;
	unsigned int pt_levels, num_did, n;
	void *reg_base, *inv_queue;
	u64 base_addr;
	int err;

	/* n = roundup(log2(system_config->interrupt_limit)) */
	for (n = 0; (1UL << n) < (system_config->interrupt_limit); n++)
		; /* empty loop */
	if (n >= 16)
		return -EINVAL;

	size = PAGE_ALIGN(sizeof(union vtd_irte) << n);
	int_remap_table = page_alloc(&mem_pool, size / PAGE_SIZE);
	if (!int_remap_table)
		return -ENOMEM;

	int_remap_table_size_log2 = n;

	for (n = 0; n < JAILHOUSE_MAX_DMAR_UNITS; n++) {
		base_addr = system_config->platform_info.x86.dmar_unit_base[n];
		if (base_addr == 0) {
			if (dmar_units == 0)
				//return -ENODEV;
				// HACK for QEMU
				printk("WARNING: No VT-d support found!\n");
			break;
		}

		printk("Found DMAR @%p\n", base_addr);

		reg_base = page_alloc(&remap_pool, 1);
		inv_queue = page_alloc(&mem_pool, 1);
		if (!reg_base || !inv_queue)
			return -ENOMEM;

		if (dmar_units == 0) {
			dmar_reg_base = reg_base;
			unit_inv_queue = inv_queue;
		}
		if (reg_base != dmar_reg_base + dmar_units * PAGE_SIZE ||
		    inv_queue != unit_inv_queue + dmar_units * PAGE_SIZE)
			return -ENOMEM;

		err = page_map_create(&hv_paging_structs, base_addr, PAGE_SIZE,
				      (unsigned long)reg_base,
				      PAGE_DEFAULT_FLAGS | PAGE_FLAG_UNCACHED,
				      PAGE_MAP_NON_COHERENT);
		if (err)
			return err;

		caps = mmio_read64(reg_base + VTD_CAP_REG);
		if (caps & VTD_CAP_SAGAW39)
			pt_levels = 3;
		else if (caps & VTD_CAP_SAGAW48)
			pt_levels = 4;
		else
			return -EIO;
		sllps_caps &= caps;

		if (dmar_pt_levels > 0 && dmar_pt_levels != pt_levels)
			return -EIO;
		dmar_pt_levels = pt_levels;

		if (caps & VTD_CAP_CM)
			return -EIO;

		ecaps = mmio_read64(reg_base + VTD_ECAP_REG);
		if (!(ecaps & VTD_ECAP_QI) || !(ecaps & VTD_ECAP_IR))
			return -EIO;

		if (mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_USED_CTRLS)
			return -EBUSY;

		num_did = 1 << (4 + (caps & VTD_CAP_NUM_DID_MASK) * 2);
		if (num_did < dmar_num_did)
			dmar_num_did = num_did;

		dmar_units++;

		vtd_init_unit(reg_base, inv_queue);
	}

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
	flush_cache(irte, sizeof(*irte));

	for (n = 0; n < dmar_units; n++) {
		vtd_submit_iq_request(reg_base, inv_queue, &inv_int);
		reg_base += PAGE_SIZE;
		inv_queue += PAGE_SIZE;
	}
}

static int vtd_find_int_remap_region(u16 device_id)
{
	int n;

	/* interrupt_limit is < 2^16, see vtd_init */
	for (n = 0; n < system_config->interrupt_limit; n++)
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

	for (n = 0; n < system_config->interrupt_limit; n++) {
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
	return -E2BIG;
}

static void vtd_free_int_remap_region(u16 device_id, unsigned int length)
{
	union vtd_irte free_irte = { .field.p = 0, .field.assigned = 0 };
	int pos = vtd_find_int_remap_region(device_id);

	if (pos >= 0) {
		printk("Freeing %u interrupt(s) for device %04x at index %d\n",
		       length, device_id, pos);
		while (length-- > 0)
			vtd_update_irte(pos, free_irte);
	}
}

int vtd_add_pci_device(struct cell *cell, struct pci_device *device)
{
	unsigned int max_vectors = device->info->num_msi_vectors;
	u16 bdf = device->info->bdf;
	u64 *root_entry_lo = &root_entry_table[PCI_BUS(bdf)].lo_word;
	struct vtd_entry *context_entry_table, *context_entry;
	int result;

	// HACK for QEMU
	if (dmar_units == 0)
		return 0;

	result = vtd_reserve_int_remap_region(bdf, max_vectors);
	if (result < 0)
		return result;

	if (*root_entry_lo & VTD_ROOT_PRESENT) {
		context_entry_table =
			page_map_phys2hvirt(*root_entry_lo & PAGE_MASK);
	} else {
		context_entry_table = page_alloc(&mem_pool, 1);
		if (!context_entry_table)
			goto error_nomem;
		*root_entry_lo = VTD_ROOT_PRESENT |
			page_map_hvirt2phys(context_entry_table);
		flush_cache(root_entry_lo, sizeof(u64));
	}

	context_entry = &context_entry_table[PCI_DEVFN(bdf)];
	context_entry->lo_word = VTD_CTX_PRESENT | VTD_CTX_TTYPE_MLP_UNTRANS |
		page_map_hvirt2phys(cell->vtd.pg_structs.root_table);
	context_entry->hi_word =
		(dmar_pt_levels == 3 ? VTD_CTX_AGAW_39 : VTD_CTX_AGAW_48) |
		(cell->id << VTD_CTX_DID_SHIFT);
	flush_cache(context_entry, sizeof(*context_entry));

	return 0;

error_nomem:
	vtd_free_int_remap_region(bdf, max_vectors);
	return -ENOMEM;
}

void vtd_remove_pci_device(struct pci_device *device)
{
	u16 bdf = device->info->bdf;
	u64 *root_entry_lo = &root_entry_table[PCI_BUS(bdf)].lo_word;
	struct vtd_entry *context_entry_table;
	struct vtd_entry *context_entry;
	unsigned int n;

	// HACK for QEMU
	if (dmar_units == 0)
		return;

	context_entry_table = page_map_phys2hvirt(*root_entry_lo & PAGE_MASK);
	context_entry = &context_entry_table[PCI_DEVFN(bdf)];

	context_entry->lo_word &= ~VTD_CTX_PRESENT;
	flush_cache(&context_entry->lo_word, sizeof(u64));

	vtd_free_int_remap_region(bdf, device->info->num_msi_vectors);

	for (n = 0; n < 256; n++)
		if (context_entry_table[n].lo_word & VTD_CTX_PRESENT)
			return;

	*root_entry_lo &= ~VTD_ROOT_PRESENT;
	flush_cache(root_entry_lo, sizeof(u64));
	page_free(&mem_pool, context_entry_table, 1);
}

int vtd_cell_init(struct cell *cell)
{
	const struct jailhouse_irqchip *irqchip =
		jailhouse_cell_irqchips(cell->config);
	unsigned int n;
	int result;

	// HACK for QEMU
	if (dmar_units == 0)
		return 0;

	if (cell->id >= dmar_num_did)
		return -ERANGE;

	cell->vtd.pg_structs.root_paging = vtd_paging;
	cell->vtd.pg_structs.root_table = page_alloc(&mem_pool, 1);
	if (!cell->vtd.pg_structs.root_table)
		return -ENOMEM;

	/* reserve regions for IRQ chips (if not done already) */
	for (n = 0; n < cell->config->num_irqchips; n++, irqchip++) {
		result = vtd_reserve_int_remap_region(irqchip->id,
						      IOAPIC_NUM_PINS);
		if (result < 0) {
			vtd_cell_exit(cell);
			return result;
		}
	}

	vtd_init_fault_nmi();

	return 0;
}

int vtd_map_memory_region(struct cell *cell,
			  const struct jailhouse_memory *mem)
{
	u32 flags = 0;

	// HACK for QEMU
	if (dmar_units == 0)
		return 0;

	if (!(mem->flags & JAILHOUSE_MEM_DMA))
		return 0;

	if (mem->flags & JAILHOUSE_MEM_READ)
		flags |= VTD_PAGE_READ;
	if (mem->flags & JAILHOUSE_MEM_WRITE)
		flags |= VTD_PAGE_WRITE;

	return page_map_create(&cell->vtd.pg_structs, mem->phys_start,
			       mem->size, mem->virt_start, flags,
			       PAGE_MAP_COHERENT);
}

int vtd_unmap_memory_region(struct cell *cell,
			    const struct jailhouse_memory *mem)
{
	// HACK for QEMU
	if (dmar_units == 0)
		return 0;

	if (!(mem->flags & JAILHOUSE_MEM_DMA))
		return 0;

	return page_map_destroy(&cell->vtd.pg_structs, mem->virt_start,
				mem->size, PAGE_MAP_COHERENT);
}

int vtd_map_interrupt(struct cell *cell, u16 device_id, unsigned int vector,
		      struct apic_irq_message irq_msg)
{
	u32 dest = irq_msg.destination;
	union vtd_irte irte;
	int base_index;

	// HACK for QEMU
	if (dmar_units == 0)
		return -ENOSYS;

	base_index = vtd_find_int_remap_region(device_id);
	if (base_index < 0)
		return base_index;

	if (vector >= system_config->interrupt_limit ||
	    base_index >= system_config->interrupt_limit - vector)
		return -ERANGE;

	irte = int_remap_table[base_index + vector];
	if (!irte.field.assigned || irte.field.sid != device_id)
		return -ERANGE;

	/*
	 * Validate delivery mode and destination(s).
	 * Note that we do support redirection hint only in logical
	 * destination mode.
	 */
	// TODO: Support x2APIC cluster mode
	if ((irq_msg.delivery_mode != APIC_MSG_DLVR_FIXED &&
	     irq_msg.delivery_mode != APIC_MSG_DLVR_LOWPRI) ||
	    irq_msg.dest_logical != irq_msg.redir_hint ||
	    (using_x2apic && irq_msg.dest_logical))
		return -EINVAL;
	if (irq_msg.dest_logical) {
		dest &= cell->cpu_set->bitmap[0];
		/*
		 * Linux may have programmed inactive vectors with too broad
		 * destination masks. Silently adjust them when programming the
		 * IRTE instead of failing the whole cell here.
		 */
		if (dest != irq_msg.destination && cell != &root_cell)
			return -EPERM;
	} else if (dest > APIC_MAX_PHYS_ID ||
		   !cell_owns_cpu(cell, apic_to_cpu_id[dest])) {
		return -EPERM;
	}

	irte.field.dest_logical = irq_msg.dest_logical;
	irte.field.redir_hint = irq_msg.redir_hint;
	irte.field.level_triggered = irq_msg.level_triggered;
	irte.field.delivery_mode = irq_msg.delivery_mode;
	irte.field.vector = irq_msg.vector;
	irte.field.destination = dest;
	if (!using_x2apic)
		/* xAPIC in flat mode: APIC ID in 47:40 (of 63:32) */
		irte.field.destination <<= 8;
	irte.field.sq = VTD_IRTE_SQ_VERIFY_FULL_SID;
	irte.field.svt = VTD_IRTE_SVT_VERIFY_SID_SQ;
	irte.field.p = 1;
	vtd_update_irte(base_index + vector, irte);

	return base_index + vector;
}

void vtd_cell_exit(struct cell *cell)
{
	// HACK for QEMU
	if (dmar_units == 0)
		return;

	page_free(&mem_pool, cell->vtd.pg_structs.root_table, 1);

	/*
	 * Note that reservation regions of IOAPICs won't be released because
	 * they might be shared with other cells
	 */
}

void vtd_config_commit(struct cell *cell_added_removed)
{
	void *reg_base = dmar_reg_base;
	int n;

	// HACK for QEMU
	if (dmar_units == 0)
		return;

	if (cell_added_removed && cell_added_removed != &root_cell)
		vtd_flush_domain_caches(cell_added_removed->id);
	vtd_flush_domain_caches(root_cell.id);

	if (mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_TES)
		return;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE) {
		vtd_update_gcmd_reg(reg_base, VTD_GCMD_TE, 1);
		vtd_update_gcmd_reg(reg_base, VTD_GCMD_IRE, 1);
	}
}

void vtd_shutdown(void)
{
	void *reg_base = dmar_reg_base;
	unsigned int n;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE) {
		vtd_update_gcmd_reg(reg_base, VTD_GCMD_IRE, 0);
		vtd_update_gcmd_reg(reg_base, VTD_GCMD_TE, 0);
		vtd_update_gcmd_reg(reg_base, VTD_GCMD_QIE, 0);
	}
}
