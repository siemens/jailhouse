/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_PAGING_H
#define _JAILHOUSE_ASM_PAGING_H

#include <jailhouse/types.h>
#include <jailhouse/utils.h>
#include <asm/processor.h>

#define PAGE_SHIFT		12

#define MAX_PAGE_TABLE_LEVELS	4

#define PAGE_FLAG_PRESENT	0x01
#define PAGE_FLAG_RW		0x02
#define PAGE_FLAG_US		0x04
#define PAGE_FLAG_FRAMEBUFFER	0x08	/* write-combining */
#define PAGE_FLAG_DEVICE	0x10	/* uncached */
#define PAGE_FLAG_NOEXECUTE	0x8000000000000000UL

#define PAGE_DEFAULT_FLAGS	(PAGE_FLAG_PRESENT | PAGE_FLAG_RW)
#define PAGE_READONLY_FLAGS	PAGE_FLAG_PRESENT
#define PAGE_PRESENT_FLAGS	PAGE_FLAG_PRESENT
/*
 * Set the higher physical address bits so that non-present mappings point to a
 * non-existing physical address, hardening against the L1TF disaster.
 */
#define PAGE_NONPRESENT_FLAGS	(INVALID_PHYS_ADDR & BIT_MASK(51, 30))

#define INVALID_PHYS_ADDR	(~0UL)

/**
 * Location of per-CPU temporary mapping region in hypervisor address space.
 */
#define TEMPORARY_MAPPING_BASE	0x0000008000000000UL
#define NUM_TEMPORARY_PAGES	16

#define REMAP_BASE		0xffffff8000000000UL
#define NUM_REMAP_BITMAP_PAGES	4

#define CELL_ROOT_PT_PAGES	1

#ifndef __ASSEMBLY__

typedef unsigned long *pt_entry_t;

static inline void arch_paging_flush_page_tlbs(unsigned long page_addr)
{
	asm volatile("invlpg (%0)" : : "r" (page_addr));
}

extern unsigned long cache_line_size;

static inline void arch_paging_flush_cpu_caches(void *addr, long size)
{
	for (; size > 0; size -= cache_line_size, addr += cache_line_size)
		asm volatile("clflush %0" : "+m" (*(char *)addr));
}

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PAGING_H */
