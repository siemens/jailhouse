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

#include <jailhouse/paging.h>

unsigned int cpu_parange = 0;

static bool arm_entry_valid(pt_entry_t entry, unsigned long flags)
{
	// FIXME: validate flags!
	return *entry & 1;
}

static unsigned long arm_get_entry_flags(pt_entry_t entry)
{
	/* Upper flags (contiguous hint and XN are currently ignored */
	return *entry & 0xfff;
}

static void arm_clear_entry(pt_entry_t entry)
{
	*entry = 0;
}

static bool arm_page_table_empty(page_table_t page_table)
{
	unsigned long n;
	pt_entry_t pte;

	for (n = 0, pte = page_table; n < PAGE_SIZE / sizeof(u64); n++, pte++)
		if (arm_entry_valid(pte, PTE_FLAG_VALID))
			return false;
	return true;
}

#if MAX_PAGE_TABLE_LEVELS > 3
static pt_entry_t arm_get_l0_entry(page_table_t page_table, unsigned long virt)
{
	return &page_table[(virt & L0_VADDR_MASK) >> 39];
}

static unsigned long arm_get_l0_phys(pt_entry_t pte, unsigned long virt)
{
	if ((*pte & PTE_TABLE_FLAGS) == PTE_TABLE_FLAGS)
		return INVALID_PHYS_ADDR;
	return (*pte & PTE_L0_BLOCK_ADDR_MASK) | (virt & BLOCK_512G_VADDR_MASK);
}
#endif

#if MAX_PAGE_TABLE_LEVELS > 2
static pt_entry_t arm_get_l1_entry(page_table_t page_table, unsigned long virt)
{
	return &page_table[(virt & L1_VADDR_MASK) >> 30];
}

static void arm_set_l1_block(pt_entry_t pte, unsigned long phys, unsigned long flags)
{
	*pte = ((u64)phys & PTE_L1_BLOCK_ADDR_MASK) | flags;
}

static unsigned long arm_get_l1_phys(pt_entry_t pte, unsigned long virt)
{
	if ((*pte & PTE_TABLE_FLAGS) == PTE_TABLE_FLAGS)
		return INVALID_PHYS_ADDR;
	return (*pte & PTE_L1_BLOCK_ADDR_MASK) | (virt & BLOCK_1G_VADDR_MASK);
}
#endif

static pt_entry_t arm_get_l1_alt_entry(page_table_t page_table, unsigned long virt)
{
	return &page_table[(virt & BIT_MASK(48,30)) >> 30];
}

static unsigned long arm_get_l1_alt_phys(pt_entry_t pte, unsigned long virt)
{
	if ((*pte & PTE_TABLE_FLAGS) == PTE_TABLE_FLAGS)
		return INVALID_PHYS_ADDR;
	return (*pte & BIT_MASK(48,30)) | (virt & BIT_MASK(29,0));
}

static pt_entry_t arm_get_l2_entry(page_table_t page_table, unsigned long virt)
{
	return &page_table[(virt & L2_VADDR_MASK) >> 21];
}

static pt_entry_t arm_get_l3_entry(page_table_t page_table, unsigned long virt)
{
	return &page_table[(virt & L3_VADDR_MASK) >> 12];
}

static void arm_set_l2_block(pt_entry_t pte, unsigned long phys, unsigned long flags)
{
	*pte = ((u64)phys & PTE_L2_BLOCK_ADDR_MASK) | flags;
}

static void arm_set_l3_page(pt_entry_t pte, unsigned long phys, unsigned long flags)
{
	*pte = ((u64)phys & PTE_PAGE_ADDR_MASK) | flags | PTE_FLAG_TERMINAL;
}

static void arm_set_l12_table(pt_entry_t pte, unsigned long next_pt)
{
	*pte = ((u64)next_pt & PTE_TABLE_ADDR_MASK) | PTE_TABLE_FLAGS;
}

static unsigned long arm_get_l12_table(pt_entry_t pte)
{
	return *pte & PTE_TABLE_ADDR_MASK;
}

static unsigned long arm_get_l2_phys(pt_entry_t pte, unsigned long virt)
{
	if ((*pte & PTE_TABLE_FLAGS) == PTE_TABLE_FLAGS)
		return INVALID_PHYS_ADDR;
	return (*pte & PTE_L2_BLOCK_ADDR_MASK) | (virt & BLOCK_2M_VADDR_MASK);
}

static unsigned long arm_get_l3_phys(pt_entry_t pte, unsigned long virt)
{
	if (!(*pte & PTE_FLAG_TERMINAL))
		return INVALID_PHYS_ADDR;
	return (*pte & PTE_PAGE_ADDR_MASK) | (virt & PAGE_OFFS_MASK);
}

#define ARM_PAGING_COMMON				\
		.entry_valid = arm_entry_valid,		\
		.get_flags = arm_get_entry_flags,	\
		.clear_entry = arm_clear_entry,		\
		.page_table_empty = arm_page_table_empty,

const static struct paging arm_paging[] = {
#if MAX_PAGE_TABLE_LEVELS > 3
	{
		ARM_PAGING_COMMON
		/* No block entries for level 0, so no need to set page_size */
		.get_entry = arm_get_l0_entry,
		.get_phys = arm_get_l0_phys,

		.set_next_pt = arm_set_l12_table,
		.get_next_pt = arm_get_l12_table,
	},
#endif
#if MAX_PAGE_TABLE_LEVELS > 2
	{
		ARM_PAGING_COMMON
		/* Block entry: 1GB */
		.page_size = 1024 * 1024 * 1024,
		.get_entry = arm_get_l1_entry,
		.set_terminal = arm_set_l1_block,
		.get_phys = arm_get_l1_phys,

		.set_next_pt = arm_set_l12_table,
		.get_next_pt = arm_get_l12_table,
	},
#endif
	{
		ARM_PAGING_COMMON
		/* Block entry: 2MB */
		.page_size = 2 * 1024 * 1024,
		.get_entry = arm_get_l2_entry,
		.set_terminal = arm_set_l2_block,
		.get_phys = arm_get_l2_phys,

		.set_next_pt = arm_set_l12_table,
		.get_next_pt = arm_get_l12_table,
	},
	{
		ARM_PAGING_COMMON
		/* Page entry: 4kB */
		.page_size = 4 * 1024,
		.get_entry = arm_get_l3_entry,
		.set_terminal = arm_set_l3_page,
		.get_phys = arm_get_l3_phys,
	}
};

const static struct paging arm_s2_paging_alt[] = {
	{
		ARM_PAGING_COMMON
		.get_entry = arm_get_l1_alt_entry,
		.get_phys = arm_get_l1_alt_phys,

		.set_next_pt = arm_set_l12_table,
		.get_next_pt = arm_get_l12_table,
	},
	{
		ARM_PAGING_COMMON
		/* Block entry: 2MB */
		.page_size = 2 * 1024 * 1024,
		.get_entry = arm_get_l2_entry,
		.set_terminal = arm_set_l2_block,
		.get_phys = arm_get_l2_phys,

		.set_next_pt = arm_set_l12_table,
		.get_next_pt = arm_get_l12_table,
	},
	{
		ARM_PAGING_COMMON
		/* Page entry: 4kB */
		.page_size = 4 * 1024,
		.get_entry = arm_get_l3_entry,
		.set_terminal = arm_set_l3_page,
		.get_phys = arm_get_l3_phys,
	}
};

const struct paging *cell_paging;

void arch_paging_init(void)
{
	cpu_parange = get_cpu_parange();

	if (cpu_parange < 44)
		/* 4 level page tables not supported for stage 2.
		 * We need to use multiple consecutive pages for L1 */
		cell_paging = arm_s2_paging_alt;
	else
		cell_paging = arm_paging;

	hv_paging_structs.root_paging = arm_paging;
}
