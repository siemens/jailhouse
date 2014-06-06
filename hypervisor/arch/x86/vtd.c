/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <asm/vtd.h>
#include <asm/apic.h>
#include <asm/bitops.h>

/* TODO: Support multiple segments */
static struct vtd_entry __attribute__((aligned(PAGE_SIZE)))
	root_entry_table[256];
static struct paging vtd_paging[VTD_MAX_PAGE_DIR_LEVELS];
static void *dmar_reg_base;
static unsigned int dmar_units;
static unsigned int dmar_pt_levels;
static unsigned int dmar_num_did = ~0U;
static unsigned int fault_reporting_cpu_id;

static void *vtd_iotlb_reg_base(void *reg_base)
{
	return reg_base + mmio_read64_field(reg_base + VTD_ECAP_REG,
					    VTD_ECAP_IRO_MASK) * 16;
}

static void vtd_flush_dmar_caches(void *reg_base, u64 ctx_scope,
				  u64 iotlb_scope)
{
	void *iotlb_reg_base;

	mmio_write64(reg_base + VTD_CCMD_REG, ctx_scope | VTD_CCMD_ICC);
	while (mmio_read64(reg_base + VTD_CCMD_REG) & VTD_CCMD_ICC)
		cpu_relax();

	iotlb_reg_base = vtd_iotlb_reg_base(reg_base);

	mmio_write64(iotlb_reg_base + VTD_IOTLB_REG,
		iotlb_scope | VTD_IOTLB_DW | VTD_IOTLB_DR | VTD_IOTLB_IVT |
		mmio_read64_field(iotlb_reg_base + VTD_IOTLB_REG,
				  VTD_IOTLB_R_MASK));

	while (mmio_read64(iotlb_reg_base + VTD_IOTLB_REG) & VTD_IOTLB_IVT)
		cpu_relax();
}

static void vtd_flush_domain_caches(unsigned int did)
{
	u64 iotlb_scope = VTD_IOTLB_IIRG_DOMAIN |
		((unsigned long)did << VTD_IOTLB_DID_SHIFT);
	void *reg_base = dmar_reg_base;
	unsigned int n;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE)
		vtd_flush_dmar_caches(reg_base, VTD_CCMD_CIRG_DOMAIN | did,
				      iotlb_scope);
}

static void vtd_set_next_pt(pt_entry_t pte, unsigned long next_pt)
{
	*pte = (next_pt & 0x000ffffffffff000UL) | VTD_PAGE_READ |
		VTD_PAGE_WRITE;
}

static void vtd_init_fault_nmi(void)
{
	void *reg_base = dmar_reg_base;
	struct per_cpu *cpu_data;
	unsigned int apic_id;
	int i;

	/* Assume that at least one bit is set somewhere as
	* we don't support configurations when Linux is left with no CPUs */
	for (i = 0; root_cell.cpu_set->bitmap[i] == 0; i++)
		/* Empty loop */;
	cpu_data = per_cpu(ffsl(root_cell.cpu_set->bitmap[i]));
	apic_id = cpu_data->apic_id;

	/* Save this value globally to avoid multiple reporting
	 * of the same case from different CPUs*/
	fault_reporting_cpu_id = cpu_data->cpu_id;

	for (i = 0; i < dmar_units; i++, reg_base += PAGE_SIZE) {
		/* Mask events*/
		mmio_write32_field(reg_base+VTD_FECTL_REG, VTD_FECTL_IM_MASK,
				   VTD_FECTL_IM_SET);

		/* We use xAPIC mode. Hence, TRGM and LEVEL aren't required.
		 Set Delivery Mode to NMI */
		mmio_write32(reg_base + VTD_FEDATA_REG, APIC_MSI_DATA_DM_NMI);

		/* The vector information is ignored in the case of NMI,
		* hence there's no need to set that field.
		* Redirection mode is set to use physical address by default */
		mmio_write32(reg_base + VTD_FEADDR_REG,
			((apic_id << APIC_MSI_ADDR_DESTID_SHIFT) &
			 APIC_MSI_ADDR_DESTID_MASK) | APIC_MSI_ADDR_FIXED_VAL);

		/* APIC ID can exceed 8-bit value for x2APIC mode */
		if (using_x2apic)
			mmio_write32(reg_base + VTD_FEUADDR_REG,
				     apic_id & APIC_MSI_UADDR_DESTID_MASK);

		/* Unmask events */
		mmio_write32_field(reg_base+VTD_FECTL_REG, VTD_FECTL_IM_MASK,
				   VTD_FECTL_IM_CLEAR);
	}
}

static void *vtd_get_fault_rec_reg_addr(void *reg_base)
{
	unsigned int regoffset;
	void *regaddr;

	regoffset = mmio_read64_field(reg_base + VTD_CAP_REG, VTD_CAP_FRO_MASK);
	regaddr = reg_base + 16*regoffset;

	return regaddr;
}

static void vtd_print_fault_record_reg_status(void *reg_base)
{
	unsigned int sid = mmio_read64_field(reg_base + VTD_FRCD_HIGH_REG,
					     VTD_FRCD_HIGH_SID_MASK);
	unsigned int fr = mmio_read64_field(reg_base + VTD_FRCD_HIGH_REG,
					    VTD_FRCD_HIGH_FR_MASK);
	unsigned long fi = mmio_read64_field(reg_base + VTD_FRCD_LOW_REG,
					     VTD_FRCD_LOW_FI_MASK);
	unsigned int type = mmio_read64_field(reg_base + VTD_FRCD_HIGH_REG,
					      VTD_FRCD_HIGH_TYPE_MASK);

	printk("VT-d fault event occurred:\n");
	printk(" Source Identifier (bus:dev.func): %02x:%02x.%x\n", sid >> 8,
	       (sid >> 3) & 0x1f, sid & 0x7);
	printk(" Fault Reason: 0x%x Fault Info: %x Type %d\n", fr, fi, type);
}

void vtd_check_pending_faults(struct per_cpu *cpu_data)
{
	unsigned int fr_index;
	void *reg_base = dmar_reg_base;
	unsigned int n;
	void *fault_reg_addr, *rec_reg_addr;

	if (cpu_data->cpu_id != fault_reporting_cpu_id)
		return;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE) {
		if (mmio_read32_field(reg_base + VTD_FSTS_REG,
			VTD_FSTS_PPF_MASK)) {
			fr_index = mmio_read32_field(reg_base + VTD_FSTS_REG,
						     VTD_FSTS_FRI_MASK);
			fault_reg_addr = vtd_get_fault_rec_reg_addr(reg_base);
			rec_reg_addr = fault_reg_addr + 16*fr_index;
			vtd_print_fault_record_reg_status(rec_reg_addr);

			/* Clear faults in record registers */
			mmio_write64_field(rec_reg_addr + VTD_FRCD_HIGH_REG,
				VTD_FRCD_HIGH_F_MASK, VTD_FRCD_HIGH_F_CLEAR);
		}
	}
}

static int vtd_init_fault_reporting(void *reg_base)
{
	int nfr, i;
	void *fault_reg_addr, *rec_reg_addr;

	nfr = mmio_read64_field(reg_base + VTD_CAP_REG, VTD_CAP_NFR_MASK);
	fault_reg_addr = vtd_get_fault_rec_reg_addr(reg_base);

	for (i = 0; i < nfr; i++) {
		rec_reg_addr = fault_reg_addr + 16*i;

		/* Clear record reg fault status */
		mmio_write64_field(rec_reg_addr + VTD_FRCD_HIGH_REG,
				VTD_FRCD_HIGH_F_MASK, VTD_FRCD_HIGH_F_CLEAR);
	}

	/* Clear fault overflow status */
	mmio_write32_field(reg_base + VTD_FSTS_REG, VTD_FSTS_PFO_MASK,
			VTD_FSTS_PFO_CLEAR);

	return 0;
}

int vtd_init(void)
{
	unsigned long offset, caps, sllps_caps = ~0UL;
	unsigned int pt_levels, num_did, n;
	const struct acpi_dmar_table *dmar;
	const struct acpi_dmar_drhd *drhd;
	void *reg_base = NULL;
	int err;

	dmar = (struct acpi_dmar_table *)acpi_find_table("DMAR", NULL);
	if (!dmar)
//		return -ENODEV;
		{ printk("WARNING: No VT-d support found!\n"); return 0; }

	if (sizeof(struct acpi_dmar_table) +
	    sizeof(struct acpi_dmar_drhd) > dmar->header.length)
		return -EIO;

	drhd = (struct acpi_dmar_drhd *)dmar->remap_structs;
	if (drhd->header.type != ACPI_DMAR_DRHD)
		return -EIO;

	offset = (void *)dmar->remap_structs - (void *)dmar;
	do {
		if (drhd->header.length < sizeof(struct acpi_dmar_drhd) ||
		    offset + drhd->header.length > dmar->header.length)
			return -EIO;

		/* TODO: support multiple segments */
		if (drhd->segment != 0)
			return -EIO;

		printk("Found DMAR @%p\n", drhd->register_base_addr);

		reg_base = page_alloc(&remap_pool, 1);
		if (!reg_base)
			return -ENOMEM;

		if (dmar_units == 0)
			dmar_reg_base = reg_base;
		else if (reg_base != dmar_reg_base + dmar_units * PAGE_SIZE)
			return -ENOMEM;

		err = page_map_create(&hv_paging_structs,
				      drhd->register_base_addr, PAGE_SIZE,
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

		/* We only support IOTLB registers withing the first page. */
		if (vtd_iotlb_reg_base(reg_base) >= reg_base + PAGE_SIZE)
			return -EIO;

		if (mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_TES)
			return -EBUSY;

		num_did = 1 << (4 + (caps & VTD_CAP_NUM_DID_MASK) * 2);
		if (num_did < dmar_num_did)
			dmar_num_did = num_did;

		dmar_units++;

		offset += drhd->header.length;
		drhd = (struct acpi_dmar_drhd *)
			(((void *)drhd) + drhd->header.length);

		err = vtd_init_fault_reporting(reg_base);
		if (err)
			return err;
	} while (offset < dmar->header.length &&
		 drhd->header.type == ACPI_DMAR_DRHD);

	vtd_init_fault_nmi();

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

static bool vtd_add_device_to_cell(struct cell *cell,
				   const struct jailhouse_pci_device *device)
{
	u64 root_entry_lo = root_entry_table[device->bus].lo_word;
	struct vtd_entry *context_entry_table, *context_entry;

	if (root_entry_lo & VTD_ROOT_PRESENT) {
		context_entry_table =
			page_map_phys2hvirt(root_entry_lo & PAGE_MASK);
	} else {
		context_entry_table = page_alloc(&mem_pool, 1);
		if (!context_entry_table)
			return false;
		root_entry_table[device->bus].lo_word = VTD_ROOT_PRESENT |
			page_map_hvirt2phys(context_entry_table);
		flush_cache(&root_entry_table[device->bus].lo_word,
			    sizeof(u64));
	}

	context_entry = &context_entry_table[device->devfn];
	if (context_entry->lo_word & VTD_CTX_PRESENT)
		return true;

	printk("Adding PCI device %02x:%02x.%x to cell \"%s\"\n",
	       device->bus, device->devfn >> 3, device->devfn & 7,
	       cell->config->name);

	context_entry->lo_word = VTD_CTX_PRESENT | VTD_CTX_TTYPE_MLP_UNTRANS |
		page_map_hvirt2phys(cell->vtd.pg_structs.root_table);
	context_entry->hi_word =
		(dmar_pt_levels == 3 ? VTD_CTX_AGAW_39 : VTD_CTX_AGAW_48) |
		(cell->id << VTD_CTX_DID_SHIFT);
	flush_cache(context_entry, sizeof(*context_entry));

	return true;
}

static void
vtd_remove_device_from_cell(struct cell *cell,
			    const struct jailhouse_pci_device *device)
{
	u64 root_entry_lo = root_entry_table[device->bus].lo_word;
	struct vtd_entry *context_entry_table;
	struct vtd_entry *context_entry;
	unsigned int n;

	if (!(root_entry_lo & VTD_ROOT_PRESENT))
		return;

	context_entry_table = page_map_phys2hvirt(root_entry_lo & PAGE_MASK);
	context_entry = &context_entry_table[device->devfn];

	if (!(context_entry->lo_word & VTD_CTX_PRESENT))
		return;

	printk("Removing PCI device %02x:%02x.%x from cell \"%s\"\n",
	       device->bus, device->devfn >> 3, device->devfn & 7,
	       cell->config->name);

	context_entry->lo_word &= ~VTD_CTX_PRESENT;
	flush_cache(&context_entry->lo_word, sizeof(u64));

	for (n = 0; n < 256; n++)
		if (context_entry_table[n].lo_word & VTD_CTX_PRESENT)
			return;

	root_entry_table[device->bus].lo_word &= ~VTD_ROOT_PRESENT;
	flush_cache(&root_entry_table[device->bus].lo_word, sizeof(u64));
	page_free(&mem_pool, context_entry_table, 1);
}

int vtd_cell_init(struct cell *cell)
{
	const struct jailhouse_memory *mem =
		jailhouse_cell_mem_regions(cell->config);
	const struct jailhouse_pci_device *dev =
		jailhouse_cell_pci_devices(cell->config);
	int n, err;

	// HACK for QEMU
	if (dmar_units == 0)
		return 0;

	if (cell->id >= dmar_num_did)
		return -ERANGE;

	cell->vtd.pg_structs.root_paging = vtd_paging;
	cell->vtd.pg_structs.root_table = page_alloc(&mem_pool, 1);
	if (!cell->vtd.pg_structs.root_table)
		return -ENOMEM;

	for (n = 0; n < cell->config->num_memory_regions; n++, mem++) {
		err = vtd_map_memory_region(cell, mem);
		if (err)
			/* FIXME: release vtd.pg_structs.root_table */
			return err;
	}

	for (n = 0; n < cell->config->num_pci_devices; n++) {
		vtd_remove_device_from_cell(&root_cell, &dev[n]);
		if (!vtd_add_device_to_cell(cell, &dev[n]))
			/* FIXME: release vtd.pg_structs.root_table,
			 * revert device additions*/
			return -ENOMEM;
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

static bool
vtd_return_device_to_root_cell(const struct jailhouse_pci_device *dev)
{
	const struct jailhouse_pci_device *root_cell_dev =
		jailhouse_cell_pci_devices(root_cell.config);
	unsigned int n;

	for (n = 0; n < root_cell.config->num_pci_devices; n++)
		if (root_cell_dev[n].domain == dev->domain &&
		    root_cell_dev[n].bus == dev->bus &&
		    root_cell_dev[n].devfn == dev->devfn)
			return vtd_add_device_to_cell(&root_cell,
						      &root_cell_dev[n]);
	return true;
}

void vtd_cell_exit(struct cell *cell)
{
	const struct jailhouse_pci_device *dev =
		jailhouse_cell_pci_devices(cell->config);
	unsigned int n;

	// HACK for QEMU
	if (dmar_units == 0)
		return;

	for (n = 0; n < cell->config->num_pci_devices; n++) {
		vtd_remove_device_from_cell(cell, &dev[n]);
		if (!vtd_return_device_to_root_cell(&dev[n]))
			printk("WARNING: Failed to re-assign PCI device to "
			       "root cell\n");
	}

	page_free(&mem_pool, cell->vtd.pg_structs.root_table, 1);
}

void vtd_config_commit(struct cell *cell_added_removed)
{
	void *reg_base = dmar_reg_base;
	int n;

	// HACK for QEMU
	if (dmar_units == 0)
		return;

	if (cell_added_removed)
		vtd_flush_domain_caches(cell_added_removed->id);
	vtd_flush_domain_caches(root_cell.id);

	if (mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_TES)
		return;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE) {
		mmio_write64(reg_base + VTD_RTADDR_REG,
			     page_map_hvirt2phys(root_entry_table));
		mmio_write32(reg_base + VTD_GCMD_REG, VTD_GCMD_SRTP);
		while (!(mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_SRTP))
			cpu_relax();

		vtd_flush_dmar_caches(reg_base, VTD_CCMD_CIRG_GLOBAL,
				      VTD_IOTLB_IIRG_GLOBAL);

		mmio_write32(reg_base + VTD_GCMD_REG, VTD_GCMD_TE);
		while (!(mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_TES))
			cpu_relax();
	}
}

void vtd_shutdown(void)
{
	void *reg_base = dmar_reg_base;
	unsigned int n;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE) {
		mmio_write32(reg_base + VTD_GCMD_REG, 0);
		while (mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_TES)
			cpu_relax();
	}
}
