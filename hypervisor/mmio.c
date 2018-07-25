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
	cell->mmio_locations[dst] = cell->mmio_locations[src];
	cell->mmio_handlers[dst] = cell->mmio_handlers[src];
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
	 * Advance the generation to odd value, indicating that modifications
	 * are ongoing. Commit this change via a barrier so that other CPUs
	 * will see this before we start changing any field.
	 */
	cell->mmio_generation++;
	memory_barrier();

	for (n = cell->num_mmio_regions; n > index; n--)
		copy_region(cell, n - 1, n);

	cell->mmio_locations[index].start = start;
	cell->mmio_locations[index].size = size;
	cell->mmio_handlers[index].function = handler;
	cell->mmio_handlers[index].arg = handler_arg;

	cell->num_mmio_regions++;

	/* Ensure all fields are committed before advancing the generation. */
	memory_barrier();
	cell->mmio_generation++;

	spin_unlock(&cell->mmio_region_lock);
}

static int find_region(struct cell *cell, unsigned long address,
		       unsigned int size, unsigned long *region_base,
		       struct mmio_region_handler *handler)
{
	unsigned int range_start, range_size, index;
	struct mmio_region_location region;
	unsigned long generation;

restart:
	generation = cell->mmio_generation;

	/*
	 * Ensure that the generation value was read prior to reading any other
	 * field.
	 */
	memory_load_barrier();

	/* Odd number? Then we have an ongoing modification and must restart. */
	if (generation & 1) {
		cpu_relax();
		goto restart;
	}

	range_start = 0;
	range_size = cell->num_mmio_regions;

	while (range_size > 0) {
		index = range_start + range_size / 2;
		region = cell->mmio_locations[index];

		/*
		 * Ensure the region location was read prior to checking the
		 * generation again.
		 */
		memory_load_barrier();

		/*
		 * If the generation changed meanwhile, the fields we read
		 * could have been inconsistent.
		 */
		if (cell->mmio_generation != generation)
			goto restart;

		if (address < region.start) {
			range_size = index - range_start;
		} else if (region.start + region.size < address + size) {
			range_size -= index + 1 - range_start;
			range_start = index + 1;
		} else {
			if (region_base != NULL) {
				*region_base = region.start;
				*handler = cell->mmio_handlers[index];
			}

			/*
			 * Ensure everything was read prior to checking the
			 * generation for the last time.
			 */
			memory_load_barrier();

			/* final check of consistency */
			if (cell->mmio_generation != generation)
				goto restart;

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

	index = find_region(cell, start, 1, NULL, NULL);
	if (index >= 0) {
		/*
		 * Advance the generation to odd value, indicating that
		 * modifications are ongoing. Commit this change via a barrier
		 * so that other CPUs will see it before we start.
		 */
		cell->mmio_generation++;
		memory_barrier();

		for (/* empty */; index < cell->num_mmio_regions; index++)
			copy_region(cell, index + 1, index);

		cell->num_mmio_regions--;

		/*
		 * Ensure all regions and their number are committed before
		 * advancing the generation.
		 */
		memory_barrier();
		cell->mmio_generation++;
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
	struct mmio_region_handler handler;
	unsigned long region_base;

	if (find_region(this_cell(), mmio->address, mmio->size, &region_base,
			&handler) < 0)
		return MMIO_UNHANDLED;

	mmio->address -= region_base;
	return handler.function(handler.arg, mmio);
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

	err = paging_create(&this_cpu_data()->pg_structs, page_phys, PAGE_SIZE,
			    TEMPORARY_MAPPING_BASE,
			    PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE,
			    PAGING_NON_COHERENT);
	if (err)
		goto invalid_access;

	/*
	 * This virt_base gives the following effective virtual address in
	 * mmio_perform_access:
	 *
	 *     TEMPORARY_MAPPING_BASE + (mem->phys_start & ~PAGE_MASK) +
	 *         (mmio->address & ~PAGE_MASK)
	 *
	 * Reason: mmio_perform_access does addr = base + mmio->address.
	 */
	virt_base = TEMPORARY_MAPPING_BASE + (mem->phys_start & ~PAGE_MASK) -
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
