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

pgd_t *hv_page_table;

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

unsigned long page_map_virt2phys(pgd_t *page_table, unsigned long virt,
				 unsigned int levels)
{
	unsigned long offs = hypervisor_header.page_offset;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	switch (levels) {
	case 4:
		pgd = pgd_offset(page_table, virt);
		if (!pgd_valid(pgd))
			return INVALID_PHYS_ADDR;

		pud = pud4l_offset(pgd, offs, virt);
		break;
	case 3:
		pud = pud3l_offset(page_table, virt);
		break;
	default:
		return INVALID_PHYS_ADDR;
	}
	if (!pud_valid(pud))
		return INVALID_PHYS_ADDR;

	pmd = pmd_offset(pud, offs, virt);
	if (!pmd_valid(pud))
		return INVALID_PHYS_ADDR;

	if (pmd_is_hugepage(pmd))
		return phys_address_hugepage(pmd, virt);

	pte = pte_offset(pmd, offs, virt);
	if (!pte_valid(pte))
		return INVALID_PHYS_ADDR;

	return phys_address(pte, virt);
}

static void flush_page_table(void *addr, unsigned long size,
			     enum page_map_coherent coherent)
{
	if (coherent == PAGE_MAP_COHERENT)
		flush_cache(addr, size);
}

int page_map_create(pgd_t *page_table, unsigned long phys, unsigned long size,
		    unsigned long virt, unsigned long flags,
		    unsigned long table_flags, unsigned int levels,
		    enum page_map_coherent coherent)
{
	unsigned long offs = hypervisor_header.page_offset;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	for (size = PAGE_ALIGN(size); size > 0;
	     phys += PAGE_SIZE, virt += PAGE_SIZE, size -= PAGE_SIZE) {
		switch (levels) {
		case 4:
			pgd = pgd_offset(page_table, virt);
			if (!pgd_valid(pgd)) {
				pud = page_alloc(&mem_pool, 1);
				if (!pud)
					return -ENOMEM;
				set_pgd(pgd, page_map_hvirt2phys(pud),
					table_flags);
				flush_page_table(pgd, sizeof(pgd), coherent);
			}
			pud = pud4l_offset(pgd, offs, virt);
			break;
		case 3:
			pud = pud3l_offset(page_table, virt);
			break;
		default:
			return -EINVAL;
		}

		if (!pud_valid(pud)) {
			pmd = page_alloc(&mem_pool, 1);
			if (!pmd)
				return -ENOMEM;
			set_pud(pud, page_map_hvirt2phys(pmd), table_flags);
			flush_page_table(pud, sizeof(pud), coherent);
		}

		pmd = pmd_offset(pud, offs, virt);
		if (!pmd_valid(pmd)) {
			pte = page_alloc(&mem_pool, 1);
			if (!pte)
				return -ENOMEM;
			set_pmd(pmd, page_map_hvirt2phys(pte), table_flags);
			flush_page_table(pmd, sizeof(pmd), coherent);
		}

		pte = pte_offset(pmd, offs, virt);
		set_pte(pte, phys, flags);
		flush_page_table(pte, sizeof(pte), coherent);
		arch_tlb_flush_page(virt);
	}

	return 0;
}

void page_map_destroy(pgd_t *page_table, unsigned long virt,
		      unsigned long size, unsigned int levels,
		      enum page_map_coherent coherent)
{
	unsigned long offs = hypervisor_header.page_offset;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	for (size = PAGE_ALIGN(size); size > 0;
	     virt += PAGE_SIZE, size -= PAGE_SIZE) {
		switch (levels) {
		case 4:
			pgd = pgd_offset(page_table, virt);
			if (!pgd_valid(pgd))
				continue;

			pud = pud4l_offset(pgd, offs, virt);
			break;
		case 3:
			pgd = 0; /* silence compiler warning */
			pud = pud3l_offset(page_table, virt);
			break;
		default:
			return;
		}
		if (!pud_valid(pud))
			continue;

		pmd = pmd_offset(pud, offs, virt);
		if (!pmd_valid(pmd))
			continue;

		pte = pte_offset(pmd, offs, virt);
		clear_pte(pte);
		flush_page_table(pte, sizeof(pte), coherent);

		if (!pt_empty(pmd, offs))
			continue;
		page_free(&mem_pool, pte_offset(pmd, offs, 0), 1);
		clear_pmd(pmd);
		flush_page_table(pmd, sizeof(pmd), coherent);

		if (!pmd_empty(pud, offs))
			continue;
		page_free(&mem_pool, pmd_offset(pud, offs, 0), 1);
		clear_pud(pud);
		flush_page_table(pud, sizeof(pud), coherent);

		if (levels < 4 || !pud_empty(pgd, offs))
			continue;
		page_free(&mem_pool, pud4l_offset(pgd, offs, 0), 1);
		clear_pgd(pgd);
		flush_page_table(pgd, sizeof(pgd), coherent);

		arch_tlb_flush_page(virt);
	}
}

void *page_map_get_foreign_page(struct per_cpu *cpu_data,
				unsigned long page_table_paddr,
				unsigned long virt, unsigned long flags)
{
	unsigned long page_virt, phys;
#if PAGE_DIR_LEVELS == 4
	pgd_t *pgd;
#endif
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int err;

	page_virt = FOREIGN_MAPPING_BASE +
		cpu_data->cpu_id * PAGE_SIZE * NUM_FOREIGN_PAGES;

	phys = arch_page_map_gphys2phys(cpu_data, page_table_paddr);
	if (phys == INVALID_PHYS_ADDR)
		return NULL;
	err = page_map_create(hv_page_table, phys, PAGE_SIZE, page_virt,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS, PAGE_MAP_NON_COHERENT);
	if (err)
		return NULL;

#if PAGE_DIR_LEVELS == 4
	pgd = pgd_offset((pgd_t *)page_virt, virt);
	if (!pgd_valid(pgd))
		return NULL;
	phys = arch_page_map_gphys2phys(cpu_data,
			(unsigned long)pud4l_offset(pgd, 0, 0));
	if (phys == INVALID_PHYS_ADDR)
		return NULL;
	err = page_map_create(hv_page_table, phys, PAGE_SIZE, page_virt,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS, PAGE_MAP_NON_COHERENT);
	if (err)
		return NULL;

	pud = pud4l_offset((pgd_t *)&page_virt, 0, virt);
#elif PAGE_DIR_LEVELS == 3
	pud = pud3l_offset((pgd_t *)page_virt, virt);
#else
# error Unsupported paging level
#endif
	if (!pud_valid(pud))
		return NULL;
	phys = arch_page_map_gphys2phys(cpu_data,
					(unsigned long)pmd_offset(pud, 0, 0));
	if (phys == INVALID_PHYS_ADDR)
		return NULL;
	err = page_map_create(hv_page_table, phys, PAGE_SIZE, page_virt,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS, PAGE_MAP_NON_COHERENT);
	if (err)
		return NULL;

	pmd = pmd_offset((pud_t *)&page_virt, 0, virt);
	if (!pmd_valid(pmd))
		return NULL;
	if (pmd_is_hugepage(pmd))
		phys = phys_address_hugepage(pmd, virt);
	else {
		phys = arch_page_map_gphys2phys(cpu_data,
				(unsigned long)pte_offset(pmd, 0, 0));
		if (phys == INVALID_PHYS_ADDR)
			return NULL;
		err = page_map_create(hv_page_table, phys, PAGE_SIZE,
				      page_virt, PAGE_READONLY_FLAGS,
				      PAGE_DEFAULT_FLAGS, PAGE_DIR_LEVELS,
				      PAGE_MAP_NON_COHERENT);
		if (err)
			return NULL;

		pte = pte_offset((pmd_t *)&page_virt, 0, virt);
		if (!pte_valid(pte))
			return NULL;
		phys = phys_address(pte, 0);
	}
	phys = arch_page_map_gphys2phys(cpu_data, phys);
	if (phys == INVALID_PHYS_ADDR)
		return NULL;

	err = page_map_create(hv_page_table, phys, PAGE_SIZE, page_virt,
			      flags, PAGE_DEFAULT_FLAGS, PAGE_DIR_LEVELS,
			      PAGE_MAP_NON_COHERENT);
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
		hypervisor_header.possible_cpus * NUM_FOREIGN_PAGES;
	for (n = 0; n < remap_pool.used_pages; n++)
		set_bit(n, remap_pool.used_bitmap);

	hv_page_table = page_alloc(&mem_pool, 1);
	if (!hv_page_table)
		goto error_nomem;

	/* Replicate hypervisor mapping of Linux */
	err = page_map_create(hv_page_table,
			      page_map_hvirt2phys(&hypervisor_header),
			      hypervisor_header.size,
			      (unsigned long)&hypervisor_header,
			      PAGE_DEFAULT_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS, PAGE_MAP_NON_COHERENT);
	if (err)
		goto error_nomem;

	/* Make sure any remappings to the foreign regions can be performed
	 * without allocations of page table pages. */
	err = page_map_create(hv_page_table, 0,
			      remap_pool.used_pages * PAGE_SIZE,
			      FOREIGN_MAPPING_BASE, PAGE_NONPRESENT_FLAGS,
			      PAGE_DEFAULT_FLAGS, PAGE_DIR_LEVELS,
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
