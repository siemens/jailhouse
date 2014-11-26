/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

#define PG_PRESENT	0x01
#define PG_RW		0x02
#define PG_PS		0x80
#define PG_PCD		0x10

static unsigned long heap_pos = HEAP_BASE;

void *alloc(unsigned long size, unsigned long align)
{
	unsigned long base = (heap_pos + align - 1) & ~(align - 1);

	heap_pos = base + size;
	return (void *)base;
}

void map_range(void *start, unsigned long size, enum map_type map_type)
{
	unsigned long pt_addr, *pt_entry, *pt;
	unsigned long vaddr = (unsigned long)start;

	asm volatile("mov %%cr3,%0" : "=r" (pt_addr));

	size += (vaddr & ~HUGE_PAGE_MASK) + HUGE_PAGE_SIZE - 1;
	size &= HUGE_PAGE_MASK;
	while (size > 0) {
#ifdef __x86_64__
		pt_addr &= PAGE_MASK;
		pt = (unsigned long *)pt_addr;

		pt_entry = &pt[(vaddr >> 39) & 0x1ff];
		if (*pt_entry & PG_PRESENT) {
			pt = (unsigned long *)(*pt_entry & PAGE_MASK);
		} else {
			pt = alloc(PAGE_SIZE, PAGE_SIZE);
			*pt_entry = (unsigned long)pt | PG_RW | PG_PRESENT;
		}

		pt_entry = &pt[(vaddr >> 30) & 0x1ff];
		if (*pt_entry & PG_PRESENT) {
			pt = (unsigned long *)(*pt_entry & PAGE_MASK);
		} else {
			pt = alloc(PAGE_SIZE, PAGE_SIZE);
			*pt_entry = (unsigned long)pt | PG_RW | PG_PRESENT;
		}

		pt_entry = &pt[(vaddr >> 21) & 0x1ff];
		*pt_entry = (vaddr & HUGE_PAGE_MASK) |
			(map_type == MAP_UNCACHED ? PG_PCD : 0) |
			PG_PS | PG_RW | PG_PRESENT;
#else
#error not yet implemented
#endif
		size -= HUGE_PAGE_SIZE;
		vaddr += HUGE_PAGE_SIZE;
	}
}
