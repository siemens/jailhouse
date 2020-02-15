/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2020
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

#define PAGE_SHIFT		12

#define MAX_PAGE_TABLE_LEVELS	3

#define PAGE_FLAG_FRAMEBUFFER	0
#define PAGE_FLAG_DEVICE	0

#define PAGE_DEFAULT_FLAGS	0
#define PAGE_READONLY_FLAGS	0
#define PAGE_PRESENT_FLAGS	0
#define PAGE_NONPRESENT_FLAGS	0

#define INVALID_PHYS_ADDR	(~0UL)

#define TEMPORARY_MAPPING_BASE	0x0000008000000000UL
#define NUM_TEMPORARY_PAGES	16

#define REMAP_BASE		0xffffff8000000000UL
#define NUM_REMAP_BITMAP_PAGES	4

#define CELL_ROOT_PT_PAGES	1

#ifndef __ASSEMBLY__

typedef u64 *pt_entry_t;

static inline void arch_paging_flush_page_tlbs(unsigned long page_addr)
{
}

static inline void arch_paging_flush_cpu_caches(void *addr, long size)
{
}

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PAGING_H */
