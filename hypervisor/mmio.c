/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2015, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/cell.h>
#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/unit.h>
#include <jailhouse/percpu.h>

/**
 * Perform MMIO-specific initialization for a new cell.
 * @param cell		Cell to be initialized.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see mmio_cell_exit
 */
int mmio_cell_init(struct cell *cell)
{
	const struct jailhouse_memory *mem;
	const struct unit *unit;
	unsigned int n;
	void *pages;

	/* cell is zero-initialized */;
	for_each_unit(unit)
		cell->max_mmio_regions += unit->mmio_count_regions(cell);

	for_each_mem_region(mem, cell->config, n)
		if (JAILHOUSE_MEMORY_IS_SUBPAGE(mem))
			cell->max_mmio_regions++;

	pages = page_alloc(&mem_pool,
			   PAGES(cell->max_mmio_regions *
				 (sizeof(struct mmio_region_location) +
				  sizeof(struct mmio_region_handler))));
	if (!pages)
		return -ENOMEM;

	cell->mmio_locations = pages;
	cell->mmio_handlers = pages +
		cell->max_mmio_regions * sizeof(struct mmio_region_location);

	return 0;
}

static void copy_region(struct cell *cell, unsigned int src, unsigned dst)
{
	/*
	 * Invalidate destination region by shrinking it to size 0. This has to
	 * be made visible to other CPUs via a memory barrier before
	 * manipulating other destination fields.
	 */
	cell->mmio_locations[dst].size = 0;
	memory_barrier();

	cell->mmio_locations[dst].start = cell->mmio_locations[src].start;
	cell->mmio_handlers[dst] = cell->mmio_handlers[src];
	/* Ensure all fields are committed before activating the region. */
	memory_barrier();

	cell->mmio_locations[dst].size = cell->mmio_locations[src].size;
}

/**
 * Register a MMIO region access handler for a cell.
 * @param cell		Cell than can access the region.
 * @param start		Region start address in cell address space.
 * @param size		Region size.
 * @param handler	Access handler.
 * @param handler_arg	Opaque argument to pass to handler.
 *
 * @see mmio_region_unregister
 */
void mmio_region_register(struct cell *cell, unsigned long start,
			  unsigned long size, mmio_handler handler,
			  void *handler_arg)
{
	unsigned int index, n;

	spin_lock(&cell->mmio_region_lock);

	if (cell->num_mmio_regions >= cell->max_mmio_regions) {
		spin_unlock(&cell->mmio_region_lock);

		printk("WARNING: Overflow during MMIO region registration!\n");
		return;
	}

	for (index = 0; index < cell->num_mmio_regions; index++)
		if (cell->mmio_locations[index].start > start)
			break;

	/*
	 * Set and commit a dummy region at the end of the list so that
	 * we can safely grow it.
	 */
	cell->mmio_locations[cell->num_mmio_regions].start = -1;
	cell->mmio_locations[cell->num_mmio_regions].size = 0;
	memory_barrier();

	/*
	 * Extend region list by one so that we can start moving entries.
	 * Commit this change via a barrier so that the current last element
	 * will remain visible when moving it up.
	 */
	cell->num_mmio_regions++;
	memory_barrier();

	for (n = cell->num_mmio_regions - 1; n > index; n--)
		copy_region(cell, n - 1, n);

	/* Invalidate the new region entry first (see also copy_region()). */
	cell->mmio_locations[index].size = 0;
	memory_barrier();

	cell->mmio_locations[index].start = start;
	cell->mmio_handlers[index].handler = handler;
	cell->mmio_handlers[index].arg = handler_arg;
	/* Ensure all fields are committed before activating the region. */
	memory_barrier();

	cell->mmio_locations[index].size = size;

	spin_unlock(&cell->mmio_region_lock);
}

static int find_region(struct cell *cell, unsigned long address,
		       unsigned int size)
{
	unsigned int range_start = 0;
	unsigned int range_size = cell->num_mmio_regions;
	struct mmio_region_location region;
	unsigned int index;

	while (range_size > 0) {
		index = range_start + range_size / 2;
		region = cell->mmio_locations[index];

		if (address < region.start) {
			range_size = index - range_start;
		} else if (region.start + region.size < address + size) {
			range_size -= index + 1 - range_start;
			range_start = index + 1;
		} else {
			return index;
		}
	}
	return -1;
}

/**
 * Unregister MMIO region from a cell.
 * @param cell		Cell the region belongs to.
 * @param start		Region start address as it was passed to
 * 			mmio_region_register().
 *
 * @see mmio_region_register
 */
void mmio_region_unregister(struct cell *cell, unsigned long start)
{
	int index;

	spin_lock(&cell->mmio_region_lock);

	index = find_region(cell, start, 1);
	if (index >= 0) {
		for (/* empty */; index < cell->num_mmio_regions; index++)
			copy_region(cell, index + 1, index);

		/*
		 * Ensure the last region move is visible before shrinking the
		 * list.
		 */
		memory_barrier();

		cell->num_mmio_regions--;
	}
	spin_unlock(&cell->mmio_region_lock);
}

/**
 * Dispatch MMIO access of a cell CPU.
 * @param mmio		MMIO access description. @a mmio->value will receive the
 * 			result of a successful read access. All @a mmio fields
 * 			may have been modified on return.
 *
 * @return MMIO_HANDLED on success, MMIO_UNHANDLED if no region is registered
 * for the access address and size, or MMIO_ERROR if an access error was
 * detected.
 *
 * @see mmio_region_register
 * @see mmio_region_unregister
 */
enum mmio_result mmio_handle_access(struct mmio_access *mmio)
{
	struct cell *cell = this_cell();
	int index = find_region(cell, mmio->address, mmio->size);
	mmio_handler handler;

	if (index < 0)
		return MMIO_UNHANDLED;

	handler = cell->mmio_handlers[index].handler;
	mmio->address -= cell->mmio_locations[index].start;
	return handler(cell->mmio_handlers[index].arg, mmio);
}

/**
 * Perform MMIO-specific cleanup for a cell under destruction.
 * @param cell		Cell to be destructed.
 *
 * @see mmio_cell_init
 */
void mmio_cell_exit(struct cell *cell)
{
	page_free(&mem_pool, cell->mmio_locations,
		  PAGES(cell->max_mmio_regions *
			(sizeof(struct mmio_region_location) +
			 sizeof(struct mmio_region_handler))));
}

void mmio_perform_access(void *base, struct mmio_access *mmio)
{
	void *addr = base + mmio->address;

	if (mmio->is_write)
		switch (mmio->size) {
		case 1:
			mmio_write8(addr, mmio->value);
			break;
		case 2:
			mmio_write16(addr, mmio->value);
			break;
		case 4:
			mmio_write32(addr, mmio->value);
			break;
#if BITS_PER_LONG == 64
		case 8:
			mmio_write64(addr, mmio->value);
			break;
#endif
		}
	else
		switch (mmio->size) {
		case 1:
			mmio->value = mmio_read8(addr);
			break;
		case 2:
			mmio->value = mmio_read16(addr);
			break;
		case 4:
			mmio->value = mmio_read32(addr);
			break;
#if BITS_PER_LONG == 64
		case 8:
			mmio->value = mmio_read64(addr);
			break;
#endif
		}
}

static enum mmio_result mmio_handle_subpage(void *arg, struct mmio_access *mmio)
{
	const struct jailhouse_memory *mem = arg;
	u64 perm = mmio->is_write ? JAILHOUSE_MEM_WRITE : JAILHOUSE_MEM_READ;
	unsigned long page_virt = TEMPORARY_MAPPING_BASE +
		this_cpu_id() * PAGE_SIZE * NUM_TEMPORARY_PAGES;
	unsigned long page_phys =
		((unsigned long)mem->phys_start + mmio->address) & PAGE_MASK;
	unsigned long virt_base;
	int err;

	/* check read/write access permissions */
	if (!(mem->flags & perm))
		goto invalid_access;

	/* width bit according to access size needs to be set */
	if (!((mmio->size << JAILHOUSE_MEM_IO_WIDTH_SHIFT) & mem->flags))
		goto invalid_access;

	/* naturally unaligned access needs to be allowed explicitly */
	if (mmio->address & (mmio->size - 1) &&
	    !(mem->flags & JAILHOUSE_MEM_IO_UNALIGNED))
		goto invalid_access;

	err = paging_create(&hv_paging_structs, page_phys, PAGE_SIZE,
			    page_virt, PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE,
			    PAGING_NON_COHERENT);
	if (err)
		goto invalid_access;

	/*
	 * This virt_base gives the following effective virtual address in
	 * mmio_perform_access:
	 *
	 *     page_virt + (mem->phys_start & ~PAGE_MASK) +
	 *         (mmio->address & ~PAGE_MASK)
	 *
	 * Reason: mmio_perform_access does addr = base + mmio->address.
	 */
	virt_base = page_virt + (mem->phys_start & ~PAGE_MASK) -
		(mmio->address & PAGE_MASK);
	mmio_perform_access((void *)virt_base, mmio);
	return MMIO_HANDLED;

invalid_access:
	panic_printk("FATAL: Invalid MMIO %s, address: %lx, size: %x\n",
		     mmio->is_write ? "write" : "read",
		     (unsigned long)mem->phys_start + mmio->address,
		     mmio->size);
	return MMIO_ERROR;
}

int mmio_subpage_register(struct cell *cell, const struct jailhouse_memory *mem)
{
	mmio_region_register(cell, mem->virt_start, mem->size,
			     mmio_handle_subpage, (void *)mem);
	return 0;
}

void mmio_subpage_unregister(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	mmio_region_unregister(cell, mem->virt_start);
}
