/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/paging.h>
#include <jailhouse/string.h>
#include <asm/vcpu.h>

#define X86_FLAG_HUGEPAGE	0x80

static struct paging hv_paging[MAX_PAGE_TABLE_LEVELS];

static bool x86_64_entry_valid(pt_entry_t pte, unsigned long flags)
{
	return (*pte & flags) == flags;
}

static unsigned long x86_64_get_flags(pt_entry_t pte)
{
	return *pte & (BIT_MASK(63, 52) | BIT_MASK(11, 8) | BIT_MASK(6, 0));
}

static void x86_64_set_next_pt(pt_entry_t pte, unsigned long next_pt)
{
	*pte = (next_pt & BIT_MASK(51, 12)) | PAGE_DEFAULT_FLAGS;
}

static void x86_64_clear_entry(pt_entry_t pte)
{
	*pte = PAGE_NONPRESENT_FLAGS;
}

static bool x86_64_page_table_empty(page_table_t page_table)
{
	pt_entry_t pte;
	unsigned int n;

	for (n = 0, pte = page_table; n < PAGE_SIZE / sizeof(u64); n++, pte++)
		if (x86_64_entry_valid(pte, PAGE_FLAG_PRESENT))
			return false;
	return true;
}

static pt_entry_t x86_64_get_entry_l4(page_table_t page_table,
				      unsigned long virt)
{
	return &page_table[(virt >> 39) & 0x1ff];
}

static pt_entry_t x86_64_get_entry_l3(page_table_t page_table,
				      unsigned long virt)
{
	return &page_table[(virt >> 30) & 0x1ff];
}

static pt_entry_t x86_64_get_entry_l2(page_table_t page_table,
				      unsigned long virt)
{
	return &page_table[(virt >> 21) & 0x1ff];
}

static pt_entry_t x86_64_get_entry_l1(page_table_t page_table,
				      unsigned long virt)
{
	return &page_table[(virt >> 12) & 0x1ff];
}

static void x86_64_set_terminal_l3(pt_entry_t pte, unsigned long phys,
				   unsigned long flags)
{
	*pte = (phys & BIT_MASK(51, 30)) | X86_FLAG_HUGEPAGE | flags;
}

static void x86_64_set_terminal_l2(pt_entry_t pte, unsigned long phys,
				   unsigned long flags)
{
	*pte = (phys & BIT_MASK(51, 21)) | X86_FLAG_HUGEPAGE | flags;
}

static void x86_64_set_terminal_l1(pt_entry_t pte, unsigned long phys,
				   unsigned long flags)
{
	*pte = (phys & BIT_MASK(51, 12)) | flags;
}

static unsigned long x86_64_get_phys_l3(pt_entry_t pte, unsigned long virt)
{
	if (!(*pte & X86_FLAG_HUGEPAGE))
		return INVALID_PHYS_ADDR;
	return (*pte & BIT_MASK(51, 30)) | (virt & BIT_MASK(29, 0));
}

static unsigned long x86_64_get_phys_l2(pt_entry_t pte, unsigned long virt)
{
	if (!(*pte & X86_FLAG_HUGEPAGE))
		return INVALID_PHYS_ADDR;
	return (*pte & BIT_MASK(51, 21)) | (virt & BIT_MASK(20, 0));
}

static unsigned long x86_64_get_phys_l1(pt_entry_t pte, unsigned long virt)
{
	return (*pte & BIT_MASK(51, 12)) | (virt & BIT_MASK(11, 0));
}

static unsigned long x86_64_get_next_pt(pt_entry_t pte)
{
	return *pte & BIT_MASK(51, 12);
}

#define X86_64_PAGING_COMMON					\
	.entry_valid		= x86_64_entry_valid,		\
	.get_flags		= x86_64_get_flags,		\
	.set_next_pt		= x86_64_set_next_pt,		\
	.clear_entry		= x86_64_clear_entry,		\
	.page_table_empty	= x86_64_page_table_empty

const struct paging x86_64_paging[] = {
	{
		X86_64_PAGING_COMMON,
		.get_entry	= x86_64_get_entry_l4,
		/* set_terminal not valid */
		.get_phys	= paging_get_phys_invalid,
		.get_next_pt	= x86_64_get_next_pt,
	},
	{
		.page_size	= 1024 * 1024 * 1024,
		X86_64_PAGING_COMMON,
		.get_entry	= x86_64_get_entry_l3,
		.set_terminal	= x86_64_set_terminal_l3,
		.get_phys	= x86_64_get_phys_l3,
		.get_next_pt	= x86_64_get_next_pt,
	},
	{
		.page_size	= 2 * 1024 * 1024,
		X86_64_PAGING_COMMON,
		.get_entry	= x86_64_get_entry_l2,
		.set_terminal	= x86_64_set_terminal_l2,
		.get_phys	= x86_64_get_phys_l2,
		.get_next_pt	= x86_64_get_next_pt,
	},
	{
		.page_size	= PAGE_SIZE,
		X86_64_PAGING_COMMON,
		.get_entry	= x86_64_get_entry_l1,
		.set_terminal	= x86_64_set_terminal_l1,
		.get_phys	= x86_64_get_phys_l1,
		/* get_next_pt not valid */
	},
};

void arch_paging_init(void)
{
	memcpy(hv_paging, x86_64_paging, sizeof(x86_64_paging));
	if (!(cpuid_edx(0x80000001, 0) & X86_FEATURE_GBPAGES))
		hv_paging[1].page_size = 0;

	hv_paging_structs.root_paging = hv_paging;
}

static bool i386_entry_valid(pt_entry_t pte, unsigned long flags)
{
	return (*(u32 *)pte & flags) == flags;
}

static pt_entry_t i386_get_entry_l2(page_table_t page_table,
				    unsigned long virt)
{
	u32 *page_table32 = (u32 *)page_table;

	return (pt_entry_t)&page_table32[(virt >> 22) & 0x3ff];
}

static pt_entry_t i386_get_entry_l1(page_table_t page_table,
				    unsigned long virt)
{
	u32 *page_table32 = (u32 *)page_table;

	return (pt_entry_t)&page_table32[(virt >> 12) & 0x3ff];
}

static unsigned long i386_get_phys_l2(pt_entry_t pte, unsigned long virt)
{
	u32 pte32 = *(u32 *)pte;

	if (!(pte32 & X86_FLAG_HUGEPAGE))
		return INVALID_PHYS_ADDR;
	return ((unsigned long)(pte32 & BIT_MASK(16, 13)) << (32 - 13)) |
		(pte32 & BIT_MASK(31, 22)) | (virt & BIT_MASK(21, 0));
}

static unsigned long i386_get_phys_l1(pt_entry_t pte, unsigned long virt)
{
	return (*(u32 *)pte & BIT_MASK(31, 12)) | (virt & BIT_MASK(11, 0));
}

static unsigned long i386_get_next_pt(pt_entry_t pte)
{
	return *(u32 *)pte & BIT_MASK(31, 12);
}

/* read-only, no page table construction supported */
const struct paging i386_paging[] = {
	{
		.page_size	= 4 * 1024 * 1024,
		.entry_valid	= i386_entry_valid,
		.get_entry	= i386_get_entry_l2,
		.get_phys	= i386_get_phys_l2,
		.get_next_pt	= i386_get_next_pt,
	},
	{
		.page_size	= PAGE_SIZE,
		.entry_valid	= i386_entry_valid,
		.get_entry	= i386_get_entry_l1,
		.get_phys	= i386_get_phys_l1,
		/* get_next_pt not valid */
	},
};

/* Can be overridden in vendor-specific code if needed */
pt_entry_t __attribute__((weak)) vcpu_pae_get_pdpte(page_table_t page_table,
						    unsigned long virt)
{
	return &page_table[(virt >> 30) & 0x3];
}

/* read-only, no page table construction supported */
const struct paging pae_paging[] = {
	{
		.entry_valid	= x86_64_entry_valid,
		.get_entry	= vcpu_pae_get_pdpte,
		.get_phys	= paging_get_phys_invalid,
		.get_next_pt	= x86_64_get_next_pt,
	},
	{
		.page_size	= 2 * 1024 * 1024,
		.entry_valid	= x86_64_entry_valid,
		.get_entry	= x86_64_get_entry_l2,
		.get_phys	= x86_64_get_phys_l2,
		.get_next_pt	= x86_64_get_next_pt,
	},
	{
		.page_size	= PAGE_SIZE,
		.entry_valid	= x86_64_entry_valid,
		.get_entry	= x86_64_get_entry_l1,
		.get_phys	= x86_64_get_phys_l1,
		/* get_next_pt not valid */
	},
};
