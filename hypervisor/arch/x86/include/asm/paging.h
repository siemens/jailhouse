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
#define PAGE_OFFS_MASK		(PAGE_SIZE - 1)

#define MAX_PAGE_DIR_LEVELS	4

#define PAGE_FLAG_PRESENT	0x01
#define PAGE_FLAG_RW		0x02
#define PAGE_FLAG_UNCACHED	0x10

#define PAGE_DEFAULT_FLAGS	(PAGE_FLAG_PRESENT | PAGE_FLAG_RW)
#define PAGE_READONLY_FLAGS	PAGE_FLAG_PRESENT
#define PAGE_NONPRESENT_FLAGS	0

#define INVALID_PHYS_ADDR	(~0UL)

#define REMAP_BASE_ADDR		0x0000000000100000UL
#define NUM_REMAP_BITMAP_PAGES	1

#define NUM_TEMPORARY_PAGES	16

#ifndef __ASSEMBLY__

typedef unsigned long *pt_entry_t;

static inline void x86_tlb_flush_all(void)
{
	unsigned long cr4 = read_cr4();

	write_cr4(cr4 & ~X86_CR4_PGE);
	write_cr4(cr4);
}

static inline void arch_tlb_flush_page(unsigned long addr)
{
	asm volatile("invlpg (%0)" : : "r" (addr));
}

extern unsigned long cache_line_size;

static inline void flush_cache(void *addr, long size)
{
	for (; size > 0; size -= cache_line_size, addr += cache_line_size)
		asm volatile("clflush %0" : "+m" (*(char *)addr));
}

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PAGING_H */
