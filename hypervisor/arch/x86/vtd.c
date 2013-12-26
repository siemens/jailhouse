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
#include <asm/vtd.h>

/* TODO: Support multiple segments */
static struct vtd_entry __attribute__((aligned(PAGE_SIZE)))
	root_entry_table[256];
static void *dmar_reg_base;
static unsigned int dmar_units;
static unsigned dmar_pt_levels;

int vtd_init(void)
{
	const struct acpi_dmar_table *dmar;
	const struct acpi_dmar_drhd *drhd;
	unsigned int pt_levels;
	void *reg_base = NULL;
	unsigned long offset;
	unsigned long caps;
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

		err = page_map_create(hv_page_table, drhd->register_base_addr,
				      PAGE_SIZE, (unsigned long)reg_base,
				      PAGE_DEFAULT_FLAGS | PAGE_FLAG_UNCACHED,
				      PAGE_DEFAULT_FLAGS, PAGE_DIR_LEVELS);
		if (err)
			return err;

		caps = mmio_read64(reg_base + VTD_CAP_REG);
		if (caps & VTD_CAP_SAGAW39)
			pt_levels = 3;
		else if (caps & VTD_CAP_SAGAW48)
			pt_levels = 4;
		else
			return -EIO;

		if (dmar_pt_levels > 0 && dmar_pt_levels != pt_levels)
			return -EIO;
		dmar_pt_levels = pt_levels;

		if (mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_TE)
			return -EBUSY;

		dmar_units++;

		offset += drhd->header.length;
		drhd = (struct acpi_dmar_drhd *)
			(((void *)drhd) + drhd->header.length);
	} while (offset < dmar->header.length &&
		 drhd->header.type == ACPI_DMAR_DRHD);

	return 0;
}

static bool vtd_add_device_to_cell(struct cell *cell,
				   struct jailhouse_pci_device *device)
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
	}

	context_entry = &context_entry_table[device->devfn];
	context_entry->lo_word = VTD_CTX_PRESENT |
		VTD_CTX_FPD | VTD_CTX_TTYPE_MLP_UNTRANS |
		page_map_hvirt2phys(cell->vtd.page_table);
	context_entry->hi_word = (dmar_pt_levels == 3 ? VTD_CTX_AGAW_39
						      : VTD_CTX_AGAW_48) |
		((cell->id << VTD_CTX_DID_SHIFT) & VTD_CTX_DID16_MASK);

	return true;
}

int vtd_cell_init(struct cell *cell)
{
	struct jailhouse_cell_desc *config = cell->config;
	struct jailhouse_pci_device *dev;
	void *reg_base = dmar_reg_base;
	struct jailhouse_memory *mem;
	u32 page_flags;
	int n, err;

	// HACK for QEMU
	if (dmar_units == 0)
		return 0;

	cell->vtd.page_table = page_alloc(&mem_pool, 1);
	if (!cell->vtd.page_table)
		return -ENOMEM;

	mem = (void *)config + sizeof(struct jailhouse_cell_desc) +
		config->cpu_set_size;

	for (n = 0; n < config->num_memory_regions; n++, mem++) {
		if (!(mem->access_flags & JAILHOUSE_MEM_DMA))
			continue;

		page_flags = 0;
		if (mem->access_flags & JAILHOUSE_MEM_READ)
			page_flags |= VTD_PAGE_READ;
		if (mem->access_flags & JAILHOUSE_MEM_WRITE)
			page_flags |= VTD_PAGE_WRITE;

		err = page_map_create(cell->vtd.page_table, mem->phys_start,
				      mem->size, mem->virt_start, page_flags,
				      VTD_PAGE_READ | VTD_PAGE_WRITE,
				      dmar_pt_levels);
		if (err)
			/* FIXME: release vtd.page_table */
			return err;
	}

	dev = (void *)mem +
		config->num_irq_lines * sizeof(struct jailhouse_irq_line) +
		config->pio_bitmap_size;

	for (n = 0; n < config->num_pci_devices; n++)
		if (!vtd_add_device_to_cell(cell, &dev[n]))
			/* FIXME: release vtd.page_table,
			 * revert device additions*/
			return -ENOMEM;

	/* Brute-force write-back of CPU caches in case the hardware accesses
	 * translation structures non-coherently */
	asm volatile("wbinvd");

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE) {
		if (!(mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_TE)) {
			mmio_write64(reg_base + VTD_RTADDR_REG,
				     page_map_hvirt2phys(root_entry_table));
			mmio_write32(reg_base + VTD_GCMD_REG, VTD_GCMD_SRTP);
			while (!(mmio_read32(reg_base + VTD_GSTS_REG) &
				 VTD_GSTS_SRTP))
				cpu_relax();

			mmio_write32(reg_base + VTD_GCMD_REG, VTD_GCMD_TE);
		} else {
			// cache flush, somehow
		}
	}

	return 0;
}

void vtd_shutdown(void)
{
	void *reg_base = dmar_reg_base;
	unsigned int n;

	for (n = 0; n < dmar_units; n++, reg_base += PAGE_SIZE) {
		mmio_write32(reg_base + VTD_GCMD_REG, 0);
		while (mmio_read32(reg_base + VTD_GSTS_REG) & VTD_GSTS_TE)
			cpu_relax();
	}
}
