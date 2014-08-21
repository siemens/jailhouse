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

#ifndef _JAILHOUSE_PAGING_H
#define _JAILHOUSE_PAGING_H

#include <jailhouse/entry.h>
#include <asm/types.h>
#include <asm/paging.h>

#define PAGE_ALIGN(s)		(((s) + PAGE_SIZE-1) & PAGE_MASK)
#define PAGES(s)		(((s) + PAGE_SIZE-1) / PAGE_SIZE)

#define TEMPORARY_MAPPING_BASE	REMAP_BASE

struct page_pool {
	void *base_address;
	unsigned long pages;
	unsigned long used_pages;
	unsigned long *used_bitmap;
	unsigned long flags;
};

enum page_map_coherent {
	PAGE_MAP_COHERENT,
	PAGE_MAP_NON_COHERENT,
};

typedef pt_entry_t page_table_t;

struct paging {
	/** Page size of terminal entries in this level or 0 if none are
	 * supported. */
	unsigned int page_size;

	/** Get entry in given table corresponding to virt address. */
	pt_entry_t (*get_entry)(page_table_t page_table, unsigned long virt);

	/** Returns true if entry is a valid and supports the provided access
	 * flags (terminal or non-terminal). */
	bool (*entry_valid)(pt_entry_t pte, unsigned long flags);

	/** Set terminal entry to physical address and access flags. */
	void (*set_terminal)(pt_entry_t pte, unsigned long phys,
			     unsigned long flags);
	/** Extract physical address from given entry. If entry is not
	 * terminal, INVALID_PHYS_ADDR is returned. */
	unsigned long (*get_phys)(pt_entry_t pte, unsigned long virt);
	/** Extract access flags from given entry. Only valid for terminal
	 * entries. */
	unsigned long (*get_flags)(pt_entry_t pte);

	/** Set entry to physical address of next-level page table. */
	void (*set_next_pt)(pt_entry_t pte, unsigned long next_pt);
	/** Get physical address of next-level page table from entry. Only
	 * valid for non-terminal entries. */
	unsigned long (*get_next_pt)(pt_entry_t pte);

	/** Invalidate entry. */
	void (*clear_entry)(pt_entry_t pte);

	/** Returns true if given page table contains no valid entries. */
	bool (*page_table_empty)(page_table_t page_table);
};

struct paging_structures {
	const struct paging *root_paging;
	page_table_t root_table;
};

struct guest_paging_structures {
	const struct paging *root_paging;
	unsigned long root_table_gphys;
};

#include <asm/paging_modes.h>

extern unsigned long page_offset;

extern struct page_pool mem_pool;
extern struct page_pool remap_pool;

extern struct paging_structures hv_paging_structs;

unsigned long page_map_get_phys_invalid(pt_entry_t pte, unsigned long virt);

void *page_alloc(struct page_pool *pool, unsigned int num);
void page_free(struct page_pool *pool, void *first_page, unsigned int num);

static inline unsigned long page_map_hvirt2phys(const volatile void *hvirt)
{
	return (unsigned long)hvirt - page_offset;
}

static inline void *page_map_phys2hvirt(unsigned long phys)
{
	return (void *)phys + page_offset;
}

unsigned long page_map_virt2phys(const struct paging_structures *pg_structs,
				 unsigned long virt);

unsigned long arch_page_map_gphys2phys(struct per_cpu *cpu_data,
				       unsigned long gphys);

int page_map_create(const struct paging_structures *pg_structs,
		    unsigned long phys, unsigned long size, unsigned long virt,
		    unsigned long flags, enum page_map_coherent coherent);
int page_map_destroy(const struct paging_structures *pg_structs,
		     unsigned long virt, unsigned long size,
		     enum page_map_coherent coherent);

void *
page_map_get_guest_pages(const struct guest_paging_structures *pg_structs,
			 unsigned long gaddr, unsigned int num,
			 unsigned long flags);

int paging_init(void);
void arch_paging_init(void);
void page_map_dump_stats(const char *when);

#endif /* !_JAILHOUSE_PAGING_H */
