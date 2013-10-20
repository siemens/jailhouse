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

#ifndef _JAILHOUSE_ASM_PAGING_H
#define _JAILHOUSE_ASM_PAGING_H

#include <asm/types.h>
#include <asm/processor.h>

#define PAGE_SIZE		4096
#define PAGE_MASK		~(PAGE_SIZE - 1)

#define PAGE_DIR_LEVELS		4

#define PAGE_TABLE_OFFS_MASK	0x00000ff8UL
#define PAGE_ADDR_MASK		0xfffff000UL
#define PAGE_OFFS_MASK		0x00000fffUL
#define HUGEPAGE_ADDR_MASK	0xffe00000UL
#define HUGEPAGE_OFFS_MASK	0x001fffffUL

#define PAGE_FLAG_PRESENT	0x01
#define PAGE_FLAG_RW		0x02
#define PAGE_FLAG_SUPERVISOR	0x04
#define PAGE_FLAG_UNCACHED	0x10

#define PAGE_DEFAULT_FLAGS	(PAGE_FLAG_PRESENT | PAGE_FLAG_RW | \
				 PAGE_FLAG_SUPERVISOR)
#define PAGE_READONLY_FLAGS	(PAGE_FLAG_PRESENT | PAGE_FLAG_SUPERVISOR)

#define INVALID_PHYS_ADDR	(~0UL)

#define REMAP_BASE_ADDR		0x00100000UL
#define NUM_REMAP_BITMAP_PAGES	1

#define FOREIGN_MAPPING_BASE	REMAP_BASE_ADDR
#define NUM_FOREIGN_PAGES	16

#ifndef __ASSEMBLY__

typedef unsigned long pgd_t;
typedef unsigned long pud_t;
typedef unsigned long pmd_t;
typedef unsigned long pte_t;

static inline bool pgd_valid(pgd_t *pgd)
{
	return *pgd & 1;
}

static inline pgd_t *pgd_offset(pgd_t *page_table, unsigned long addr)
{
	return NULL;
}

static inline void set_pgd(pgd_t *pgd, unsigned long addr, unsigned long flags)
{
	*pgd = (addr & PAGE_ADDR_MASK) | flags;
}

static inline void clear_pgd(pgd_t *pgd)
{
	*pgd = 0;
}

static inline bool pud_valid(pud_t *pud)
{
	return *pud & 1;
}

static inline pud_t *pud4l_offset(pgd_t *pgd, unsigned long page_table_offset,
				  unsigned long addr)
{
	return NULL;
}

static inline pud_t *pud3l_offset(pgd_t *page_table, unsigned long addr)
{
	return NULL;
}

static inline void set_pud(pud_t *pud, unsigned long addr, unsigned long flags)
{
	*pud = (addr & PAGE_ADDR_MASK) | flags;
}

static inline void clear_pud(pud_t *pud)
{
	*pud = 0;
}

static inline bool pmd_valid(pmd_t *pmd)
{
	return *pmd & 1;
}

static inline bool pmd_is_hugepage(pmd_t *pmd)
{
	return *pmd & (1 << 7);
}

static inline pmd_t *pmd_offset(pud_t *pud, unsigned long page_table_offset,
				unsigned long addr)
{
	return NULL;
}

static inline void set_pmd(pmd_t *pmd, unsigned long addr, unsigned long flags)
{
	*pmd = (addr & PAGE_ADDR_MASK) | flags;
}

static inline void clear_pmd(pmd_t *pmd)
{
	*pmd = 0;
}

static inline bool pte_valid(pte_t *pte)
{
	return *pte & 1;
}

static inline pte_t *pte_offset(pmd_t *pmd, unsigned long page_table_offset,
				unsigned long addr)
{
	return NULL;
}

static inline void set_pte(pte_t *pte, unsigned long addr, unsigned long flags)
{
	*pte = (addr & PAGE_ADDR_MASK) | flags;
}

static inline void clear_pte(pte_t *pte)
{
	*pte = 0;
}

static inline unsigned long phys_address(pte_t *pte, unsigned long addr)
{
	return (*pte & PAGE_ADDR_MASK) + (addr & PAGE_OFFS_MASK);
}

static inline unsigned long phys_address_hugepage(pmd_t *pmd,
						  unsigned long addr)
{
	return (*pmd & HUGEPAGE_ADDR_MASK) + (addr & HUGEPAGE_OFFS_MASK);
}

static inline bool pud_empty(pgd_t *pgd, unsigned long page_table_offset)
{
	pud_t *pud = (pud_t *)((*pgd & PAGE_ADDR_MASK) + page_table_offset);
	int n;

	for (n = 0; n < PAGE_SIZE / sizeof(pud_t); n++, pud++)
		if (pud_valid(pud))
			return false;
	return true;
}

static inline bool pmd_empty(pud_t *pud, unsigned long page_table_offset)
{
	pmd_t *pmd = (pmd_t *)((*pud & PAGE_ADDR_MASK) + page_table_offset);
	int n;

	for (n = 0; n < PAGE_SIZE / sizeof(pmd_t); n++, pmd++)
		if (pmd_valid(pmd))
			return false;
	return true;
}

static inline bool pt_empty(pmd_t *pmd, unsigned long page_table_offset)
{
	pte_t *pte = (pte_t *)((*pmd & PAGE_ADDR_MASK) + page_table_offset);
	int n;

	for (n = 0; n < PAGE_SIZE / sizeof(pte_t); n++, pte++)
		if (pte_valid(pte))
			return false;
	return true;
}

static inline void flush_tlb(void)
{
}

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PAGING_H */
