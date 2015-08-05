/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/cell.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <asm/percpu.h>

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
	void *pages;

	cell->max_mmio_regions = arch_mmio_count_regions(cell);

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
	 * Set and commit a dummy region at the end if the list so that
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

	index = find_region(cell, start, 0);
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
