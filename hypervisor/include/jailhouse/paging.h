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

#include <jailhouse/entry.h>
#include <asm/types.h>
#include <asm/paging.h>

#define PAGE_ALIGN(s)		((s + PAGE_SIZE-1) & PAGE_MASK)

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

extern struct page_pool mem_pool;
extern struct page_pool remap_pool;

extern pgd_t *hv_page_table;

void *page_alloc(struct page_pool *pool, unsigned int num);
void page_free(struct page_pool *pool, void *first_page, unsigned int num);

static inline unsigned long page_map_hvirt2phys(void *hvirt)
{
	return (unsigned long)hvirt - hypervisor_header.page_offset;
}

static inline void *page_map_phys2hvirt(unsigned long phys)
{
	return (void *)phys + hypervisor_header.page_offset;
}

int page_map_create(pgd_t *page_table, unsigned long phys, unsigned long size,
		    unsigned long virt, unsigned long page_flags,
		    unsigned long table_flags, unsigned int levels,
		    enum page_map_coherent coherent);
void page_map_destroy(pgd_t *page_table, unsigned long virt,
		      unsigned long size, unsigned int levels,
		      enum page_map_coherent coherent);

void *page_map_get_foreign_page(unsigned int mapping_region,
				unsigned long page_table_paddr,
				unsigned long page_table_offset,
				unsigned long virt, unsigned long flags);

int paging_init(void);
void page_map_dump_stats(const char *when);
