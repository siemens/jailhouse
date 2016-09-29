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
#include <asm/sysregs.h>
#include <asm/control.h>

int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	u64 phys_start = mem->phys_start;
	u32 flags = PTE_FLAG_VALID | PTE_ACCESS_FLAG;

	if (mem->flags & JAILHOUSE_MEM_READ)
		flags |= S2_PTE_ACCESS_RO;
	if (mem->flags & JAILHOUSE_MEM_WRITE)
		flags |= S2_PTE_ACCESS_WO;
	if (mem->flags & JAILHOUSE_MEM_IO)
		flags |= S2_PTE_FLAG_DEVICE;
	else
		flags |= S2_PTE_FLAG_NORMAL;
	if (mem->flags & JAILHOUSE_MEM_COMM_REGION)
		phys_start = paging_hvirt2phys(&cell->comm_page);
	/*
	if (!(mem->flags & JAILHOUSE_MEM_EXECUTE))
		flags |= S2_PAGE_ACCESS_XN;
	*/

	return paging_create(&cell->arch.mm, phys_start, mem->size,
			     mem->virt_start, flags, PAGING_COHERENT);
}

int arch_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	return paging_destroy(&cell->arch.mm, mem->virt_start, mem->size,
			      PAGING_COHERENT);
}

unsigned long arch_paging_gphys2phys(struct per_cpu *cpu_data,
				     unsigned long gphys, unsigned long flags)
{
	/* Translate IPA->PA */
	return paging_virt2phys(&cpu_data->cell->arch.mm, gphys, flags);
}

void arm_cell_dcaches_flush(struct cell *cell, enum dcache_flush flush)
{
	unsigned long vaddr = TEMPORARY_MAPPING_BASE +
		this_cpu_id() * PAGE_SIZE * NUM_TEMPORARY_PAGES;
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
			paging_create(&hv_paging_structs, region_addr, size,
				      vaddr, PAGE_DEFAULT_FLAGS,
				      PAGING_NON_COHERENT);

			arm_dcaches_flush((void *)vaddr, size, flush);

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
		page_alloc_aligned(&mem_pool, ARM_CELL_ROOT_PT_SZ);

	if (!cell->arch.mm.root_table)
		return -ENOMEM;

	return 0;
}

void arm_paging_cell_destroy(struct cell *cell)
{
	page_free(&mem_pool, cell->arch.mm.root_table, ARM_CELL_ROOT_PT_SZ);
}

void arm_paging_vcpu_init(struct paging_structures *pg_structs)
{
	unsigned long cell_table = paging_hvirt2phys(pg_structs->root_table);
	u64 vttbr = 0;

	vttbr |= (u64)this_cell()->config->id << VTTBR_VMID_SHIFT;
	vttbr |= (u64)(cell_table & TTBR_MASK);

	arm_write_sysreg(VTTBR_EL2, vttbr);
	arm_write_sysreg(VTCR_EL2, VTCR_CELL);

	/* Ensure that the new VMID is present before flushing the caches */
	isb();
	/*
	 * At initialisation, arch_config_commit does not act on other CPUs,
	 * since they register themselves to the root cpu_set afterwards. It
	 * means that this unconditionnal flush is redundant on master CPU.
	 */
	arm_paging_vcpu_flush_tlbs();
}
