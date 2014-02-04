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

#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/control.h>
#include <asm/bitops.h>

#define BITS_PER_PAGE		(PAGE_SIZE * 8)

#define INVALID_PAGE_NR		(~0UL)

#define PAGE_SCRUB_ON_FREE	0x1

extern u8 __page_pool[];

struct page_pool mem_pool;
struct page_pool remap_pool = {
	.base_address = (void *)REMAP_BASE_ADDR,
	.pages = BITS_PER_PAGE * NUM_REMAP_BITMAP_PAGES,
};

struct paging_structures hv_paging_structs;

unsigned long page_map_get_phys_invalid(pt_entry_t pte, unsigned long virt)
{
	return INVALID_PHYS_ADDR;
}

static unsigned long find_next_free_page(struct page_pool *pool,
					 unsigned long start)
{
	unsigned long start_mask =
		~0UL >> (BITS_PER_LONG - (start % BITS_PER_LONG));
	unsigned long bmp_pos, bmp_val, page_nr;

	if (start >= pool->pages)
		return INVALID_PAGE_NR;

	for (bmp_pos = start / BITS_PER_LONG;
	     bmp_pos < pool->pages / BITS_PER_LONG; bmp_pos++) {
		bmp_val = pool->used_bitmap[bmp_pos] | start_mask;
		start_mask = 0;
		if (bmp_val != ~0UL) {
			page_nr = ffz(bmp_val) + bmp_pos * BITS_PER_LONG;
			if (page_nr >= pool->pages)
				break;
			return page_nr;
		}
	}

	return INVALID_PAGE_NR;
}

void *page_alloc(struct page_pool *pool, unsigned int num)
{
	unsigned long start, last, next;
	unsigned int allocated;

	start = find_next_free_page(pool, 0);
	if (start == INVALID_PAGE_NR)
		return NULL;

restart:
	for (allocated = 1, last = start; allocated < num;
	     allocated++, last = next) {
		next = find_next_free_page(pool, last + 1);
		if (next == INVALID_PAGE_NR)
			return NULL;
		if (next != last + 1) {
			start = next;
			goto restart;
		}
	}

	for (allocated = 0; allocated < num; allocated++)
		set_bit(start + allocated, pool->used_bitmap);

	pool->used_pages += num;

	return pool->base_address + start * PAGE_SIZE;
}

void page_free(struct page_pool *pool, void *page, unsigned int num)
{
	unsigned long page_nr;

	if (!page)
		return;

	while (num-- > 0) {
		if (pool->flags & PAGE_SCRUB_ON_FREE)
			memset(page, 0, PAGE_SIZE);
		page_nr = (page - pool->base_address) / PAGE_SIZE;
		clear_bit(page_nr, pool->used_bitmap);
		pool->used_pages--;
		page += PAGE_SIZE;
	}
}

unsigned long page_map_virt2phys(const struct paging_structures *pg_structs,
				 unsigned long virt)
{
	const struct paging *paging = pg_structs->root_paging;
	page_table_t pt = pg_structs->root_table;
	unsigned long phys;
	pt_entry_t pte;

	while (1) {
		pte = paging->get_entry(pt, virt);
		if (!paging->entry_valid(pte))
			return INVALID_PHYS_ADDR;
		phys = paging->get_phys(pte, virt);
		if (phys != INVALID_PHYS_ADDR)
			return phys;
		pt = page_map_phys2hvirt(paging->get_next_pt(pte));
		paging++;
	}
}

static void flush_pt_entry(pt_entry_t pte, enum page_map_coherent coherent)
{
	if (coherent == PAGE_MAP_COHERENT)
		flush_cache(pte, sizeof(*pte));
}

int page_map_create(const struct paging_structures *pg_structs,
		    unsigned long phys, unsigned long size, unsigned long virt,
		    unsigned long flags, enum page_map_coherent coherent)
{
	virt &= PAGE_MASK;
	size = PAGE_ALIGN(size);

	while (size > 0) {
		const struct paging *paging = pg_structs->root_paging;
		page_table_t pt = pg_structs->root_table;
		pt_entry_t pte;

		while (1) {
			pte = paging->get_entry(pt, virt);
			if (paging->page_size > 0) {
				paging->set_terminal(pte, phys, flags);
				flush_pt_entry(pte, coherent);
				break;
			}
			if (paging->entry_valid(pte)) {
				pt = page_map_phys2hvirt(
						paging->get_next_pt(pte));
			} else {
				pt = page_alloc(&mem_pool, 1);
				if (!pt)
					return -ENOMEM;
				paging->set_next_pt(pte,
						    page_map_hvirt2phys(pt));
				flush_pt_entry(pte, coherent);
			}
			paging++;
		}
		arch_tlb_flush_page(virt);

		phys += paging->page_size;
		virt += paging->page_size;
		size -= paging->page_size;
	}
	return 0;
}

void page_map_destroy(const struct paging_structures *pg_structs,
		      unsigned long virt, unsigned long size,
		      enum page_map_coherent coherent)
{
	size = PAGE_ALIGN(size);

	while (size > 0) {
		const struct paging *paging = pg_structs->root_paging;
		page_table_t pt[MAX_PAGE_DIR_LEVELS];
		unsigned long page_size;
		pt_entry_t pte;
		int n = 0;

		/* walk down the page table, saving intermediate tables */
		pt[0] = pg_structs->root_table;
		while (1) {
			pte = paging->get_entry(pt[n], virt);
			if (!paging->entry_valid(pte))
				break;
			if (paging->get_phys(pte, virt) != INVALID_PHYS_ADDR)
				break;
			pt[++n] = page_map_phys2hvirt(
					paging->get_next_pt(pte));
			paging++;
		}
		/* advance by page size of current level paging */
		page_size = paging->page_size ? paging->page_size : PAGE_SIZE;

		/* walk up again, clearing entries, releasing empty tables */
		while (1) {
			paging->clear_entry(pte);
			flush_pt_entry(pte, coherent);
			if (n == 0 || !paging->page_table_empty(pt[n]))
				break;
			page_free(&mem_pool, pt[n], 1);
			paging--;
			pte = paging->get_entry(pt[--n], virt);
		}
		arch_tlb_flush_page(virt);

		if (page_size > size)
			break;
		virt += page_size;
		size -= page_size;
	}
}

void *page_map_get_guest_page(struct per_cpu *cpu_data,
			      const struct paging *paging,
			      unsigned long page_table_gphys,
			      unsigned long virt, unsigned long flags)
{
	unsigned long page_virt, phys;
	pt_entry_t pte;
	int err;

	page_virt = TEMPORARY_MAPPING_BASE +
		cpu_data->cpu_id * PAGE_SIZE * NUM_TEMPORARY_PAGES;

	while (1) {
		/* map guest page table */
		phys = arch_page_map_gphys2phys(cpu_data, page_table_gphys);
		if (phys == INVALID_PHYS_ADDR)
			return NULL;
		err = page_map_create(&hv_paging_structs, phys,
				      PAGE_SIZE, page_virt,
				      PAGE_READONLY_FLAGS,
				      PAGE_MAP_NON_COHERENT);
		if (err)
			return NULL;

		/* evaluate page table entry */
		pte = paging->get_entry((page_table_t)page_virt, virt);
		if (!paging->entry_valid(pte))
			return NULL;
		phys = paging->get_phys(pte, virt);
		if (phys != INVALID_PHYS_ADDR)
			break;
		page_table_gphys = paging->get_next_pt(pte);
		paging++;
	}

	/* map guest page */
	err = page_map_create(&hv_paging_structs, phys, PAGE_SIZE, page_virt,
			      flags, PAGE_MAP_NON_COHERENT);
	if (err)
		return NULL;

	return (void *)page_virt;
}

int paging_init(void)
{
	unsigned long per_cpu_pages, config_pages, bitmap_pages;
	unsigned long n;
	int err;

	mem_pool.pages = (hypervisor_header.size -
		(__page_pool - (u8 *)&hypervisor_header)) / PAGE_SIZE;
	per_cpu_pages = hypervisor_header.possible_cpus *
		sizeof(struct per_cpu) / PAGE_SIZE;
	bitmap_pages = (mem_pool.pages + BITS_PER_PAGE - 1) / BITS_PER_PAGE;

	system_config = (struct jailhouse_system *)
		(__page_pool + per_cpu_pages * PAGE_SIZE);
	config_pages = (jailhouse_system_config_size(system_config) +
			PAGE_SIZE - 1) / PAGE_SIZE;

	if (mem_pool.pages <= per_cpu_pages + config_pages + bitmap_pages)
		goto error_nomem;

	mem_pool.base_address = __page_pool;
	mem_pool.used_bitmap =
		(unsigned long *)(__page_pool + per_cpu_pages * PAGE_SIZE +
				  config_pages * PAGE_SIZE);
	mem_pool.used_pages = per_cpu_pages + config_pages + bitmap_pages;
	for (n = 0; n < mem_pool.used_pages; n++)
		set_bit(n, mem_pool.used_bitmap);
	mem_pool.flags = PAGE_SCRUB_ON_FREE;

	remap_pool.used_bitmap = page_alloc(&mem_pool, NUM_REMAP_BITMAP_PAGES);
	remap_pool.used_pages =
		hypervisor_header.possible_cpus * NUM_TEMPORARY_PAGES;
	for (n = 0; n < remap_pool.used_pages; n++)
		set_bit(n, remap_pool.used_bitmap);

	hv_paging_structs.root_paging = hv_paging;
	hv_paging_structs.root_table = page_alloc(&mem_pool, 1);
	if (!hv_paging_structs.root_table)
		goto error_nomem;

	/* Replicate hypervisor mapping of Linux */
	err = page_map_create(&hv_paging_structs,
			      page_map_hvirt2phys(&hypervisor_header),
			      hypervisor_header.size,
			      (unsigned long)&hypervisor_header,
			      PAGE_DEFAULT_FLAGS, PAGE_MAP_NON_COHERENT);
	if (err)
		goto error_nomem;

	/* Make sure any remappings to the temporary regions can be performed
	 * without allocations of page table pages. */
	err = page_map_create(&hv_paging_structs, 0,
			      remap_pool.used_pages * PAGE_SIZE,
			      TEMPORARY_MAPPING_BASE, PAGE_NONPRESENT_FLAGS,
			      PAGE_MAP_NON_COHERENT);
	if (err)
		goto error_nomem;

	return 0;

error_nomem:
	printk("FATAL: page pool much too small\n");
	return -ENOMEM;
}

void page_map_dump_stats(const char *when)
{
	printk("Page pool usage %s: mem %d/%d, remap %d/%d\n", when,
	       mem_pool.used_pages, mem_pool.pages,
	       remap_pool.used_pages, remap_pool.pages);
}
