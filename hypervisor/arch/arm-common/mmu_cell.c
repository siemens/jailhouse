/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/coloring.h>
#include <asm/sysregs.h>
#include <asm/control.h>
#include <asm/iommu.h>

int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	u64 phys_start = mem->phys_start;
	unsigned long access_flags = PTE_FLAG_VALID | PTE_ACCESS_FLAG;
	unsigned long paging_flags = PAGING_COHERENT | PAGING_HUGE;
	int err = 0;

	if (mem->flags & JAILHOUSE_MEM_READ)
		access_flags |= S2_PTE_ACCESS_RO;
	if (mem->flags & JAILHOUSE_MEM_WRITE)
		access_flags |= S2_PTE_ACCESS_WO;
	if (mem->flags & JAILHOUSE_MEM_IO)
		access_flags |= S2_PTE_FLAG_DEVICE;
	else
		access_flags |= S2_PTE_FLAG_NORMAL;
	if (mem->flags & JAILHOUSE_MEM_COMM_REGION)
		phys_start = paging_hvirt2phys(&cell->comm_page);
	/*
	if (!(mem->flags & JAILHOUSE_MEM_EXECUTE))
		flags |= S2_PAGE_ACCESS_XN;
	*/
	if (mem->flags & JAILHOUSE_MEM_NO_HUGEPAGES)
		paging_flags &= ~PAGING_HUGE;

	err = iommu_map_memory_region(cell, mem);
	if (err)
		return err;

	if (mem->flags & JAILHOUSE_MEM_COLORED)
		/**
		 * Identity mapping is necessary only when remapping to the root
		 * cell during destroy phase. To check if we are in the destroy
		 * phase the control is made on the memory virtual start and
		 * col_load_address. We cannot have a scenario where these
		 * addresses are equal because:
		 * 1) virt_start == phys_start.
		 * 2) we assume that col_load_address is configured so that it
		 * does not interfere with memory layout.
		 * 3) if col_load_address is equal to phys_start there is a
		 * wrong root-cell configuration.
		 * It means that in the previous wrong scenario col_load_address
		 * overlaps some root-cell memory space.
		 */
		err = paging_create_colored(&cell->arch.mm, phys_start,
			mem->size, mem->virt_start, access_flags, paging_flags,
			cell->arch.color_bitmask, cell == &root_cell);
	else
		err = paging_create(&cell->arch.mm, phys_start, mem->size,
			mem->virt_start, access_flags, paging_flags);
	if (err)
		iommu_unmap_memory_region(cell, mem);

	return err;
}

int arch_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	int err = 0;

	err = iommu_unmap_memory_region(cell, mem);
	if (err)
		return err;

	/*
	 * Do not be confused -- since paging_destroy* acts on virtual
	 * addresses, paging_destroy can be physically colored, too.
	 * We need to destroy the mapping using coloring only when unmapping
	 * from the root cell during cell_create so that the correct regions are
	 * removed and then used from the cells.
	 */
	if (mem->flags & JAILHOUSE_MEM_COLORED && (cell == &root_cell))
		return paging_destroy_colored(&cell->arch.mm, mem->virt_start,
			mem->size, PAGING_COHERENT, cell->arch.color_bitmask);
	else
		return paging_destroy(&cell->arch.mm, mem->virt_start,
			mem->size, PAGING_COHERENT);
}

unsigned long arch_paging_gphys2phys(unsigned long gphys, unsigned long flags)
{
	/* Translate IPA->PA */
	return paging_virt2phys(&this_cell()->arch.mm, gphys, flags);
}

void arm_cell_dcaches_flush(struct cell *cell, enum dcache_flush flush)
{
	unsigned long region_addr, region_size, size;
	struct jailhouse_memory const *mem;
	unsigned int n;

	for_each_mem_region(mem, cell->config, n) {
		if (mem->flags & (JAILHOUSE_MEM_IO | JAILHOUSE_MEM_COMM_REGION))
			continue;

		region_addr = mem->phys_start;
		region_size = mem->size;

		while (region_size > 0) {
			size = MIN(region_size,
				   NUM_TEMPORARY_PAGES * PAGE_SIZE);

			/* cannot fail, mapping area is preallocated */
			if (mem->flags & JAILHOUSE_MEM_COLORED)
				paging_create_colored(
					&this_cpu_data()->pg_structs,
					region_addr, size,
					TEMPORARY_MAPPING_BASE,
					PAGE_DEFAULT_FLAGS,
					PAGING_NON_COHERENT | PAGING_NO_HUGE,
					cell->arch.color_bitmask, false);
			else
				paging_create(&this_cpu_data()->pg_structs,
					region_addr,
					size, TEMPORARY_MAPPING_BASE,
					PAGE_DEFAULT_FLAGS,
					PAGING_NON_COHERENT | PAGING_NO_HUGE);

			arm_dcaches_flush((void *)TEMPORARY_MAPPING_BASE, size,
					  flush);

			region_addr += size;
			region_size -= size;
		}
	}

	/* ensure completion of the flush */
	dmb(ish);
}

int arm_paging_cell_init(struct cell *cell)
{
	if (cell->config->id > 0xff)
		return trace_error(-E2BIG);

	cell->arch.mm.root_paging = cell_paging;
	cell->arch.mm.root_table =
		page_alloc_aligned(&mem_pool, CELL_ROOT_PT_PAGES);

	if (!cell->arch.mm.root_table)
		return -ENOMEM;

	/* Init coloring configuration of the cell */
	return coloring_cell_init(cell);
}

void arm_paging_cell_destroy(struct cell *cell)
{
	page_free(&mem_pool, cell->arch.mm.root_table, CELL_ROOT_PT_PAGES);
}

void arm_paging_vcpu_init(struct paging_structures *pg_structs)
{
	unsigned long cell_table = paging_hvirt2phys(pg_structs->root_table);
	u64 vttbr = 0;

	vttbr |= (u64)this_cell()->config->id << VTTBR_VMID_SHIFT;
	vttbr |= (u64)(cell_table & TTBR_MASK);

	arm_write_sysreg(VTTBR_EL2, vttbr);

	/* Ensure that the new VMID is present before flushing the caches */
	isb();
	/*
	 * At initialisation, arch_config_commit does not act on other CPUs,
	 * since they register themselves to the root cpu_set afterwards. It
	 * means that this unconditionnal flush is redundant on master CPU.
	 */
	arm_paging_vcpu_flush_tlbs();
}
