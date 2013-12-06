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

#define PAGE_SCRUB_ON_FREE	0x1

extern u8 __start[], __page_pool[];

struct page_pool mem_pool;
struct page_pool remap_pool = {
	.base_address = (void *)REMAP_BASE_ADDR,
	.pages = BITS_PER_PAGE * NUM_REMAP_BITMAP_PAGES,
};

pgd_t *hv_page_table;

static void *page_alloc_one(struct page_pool *pool)
{
	unsigned long word, page_nr;

	for (word = 0; word < pool->pages / BITS_PER_LONG; word++)
		if (pool->used_bitmap[word] != ~0UL) {
			page_nr = ffz(pool->used_bitmap[word]) +
				word * BITS_PER_LONG;
			if (page_nr >= pool->pages)
				break;
			set_bit(page_nr, pool->used_bitmap);
			pool->used_pages++;
			return pool->base_address + page_nr * PAGE_SIZE;
		}

	return NULL;
}

void *page_alloc(struct page_pool *pool, unsigned int num)
{
	void *start, *last, *next;
	unsigned int allocated;

	start = page_alloc_one(pool);
	if (!start)
		return NULL;

	for (allocated = 1, last = start; allocated < num;
	     allocated++, last = next) {
		next = page_alloc_one(pool);
		if (next != last + PAGE_SIZE) {
			page_free(pool, start, allocated);
			return NULL;
		}
	}

	return start;
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

unsigned long page_map_virt2phys(pgd_t *page_table,
				 unsigned long page_table_offset,
				 unsigned long virt)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

#if PAGE_DIR_LEVELS == 4
	pgd = pgd_offset(page_table, virt);
	if (!pgd_valid(pgd))
		return INVALID_PHYS_ADDR;

	pud = pud4l_offset(pgd, page_table_offset, virt);
#elif PAGE_DIR_LEVELS == 3
	pud = pud3l_offset(pgd, page_table_offset, virt);
#else
# error Unsupported paging level
#endif
	if (!pud_valid(pud))
		return INVALID_PHYS_ADDR;

	pmd = pmd_offset(pud, page_table_offset, virt);
	if (!pmd_valid(pud))
		return INVALID_PHYS_ADDR;

	if (pmd_is_hugepage(pmd))
		return phys_address_hugepage(pmd, virt);

	pte = pte_offset(pmd, page_table_offset, virt);
	if (!pte_valid(pte))
		return INVALID_PHYS_ADDR;

	return phys_address(pte, virt);
}

int page_map_create(pgd_t *page_table, unsigned long phys, unsigned long size,
		    unsigned long virt, unsigned long flags,
		    unsigned long table_flags, unsigned int levels)
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
		}

		pmd = pmd_offset(pud, offs, virt);
		if (!pmd_valid(pmd)) {
			pte = page_alloc(&mem_pool, 1);
			if (!pte)
				return -ENOMEM;
			set_pmd(pmd, page_map_hvirt2phys(pte), table_flags);
		}

		pte = pte_offset(pmd, offs, virt);
		set_pte(pte, phys, flags);
	}

	flush_tlb();

	return 0;
}

void page_map_destroy(pgd_t *page_table, unsigned long virt,
		      unsigned long size, unsigned int levels)
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

		if (!pt_empty(pmd, offs))
			continue;
		page_free(&mem_pool, pte_offset(pmd, offs, 0), 1);
		clear_pmd(pmd);

		if (!pmd_empty(pud, offs))
			continue;
		page_free(&mem_pool, pmd_offset(pud, offs, 0), 1);
		clear_pud(pud);

		if (levels < 4 || !pud_empty(pgd, offs))
			continue;
		page_free(&mem_pool, pud4l_offset(pgd, offs, 0), 1);
		clear_pgd(pgd);
	}

	flush_tlb();
}

void *page_map_get_foreign_page(unsigned int mapping_region,
				unsigned long page_table_paddr,
				unsigned long page_table_offset,
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
		mapping_region * PAGE_SIZE * NUM_FOREIGN_PAGES;

	phys = page_table_paddr + page_table_offset;
	err = page_map_create(hv_page_table, phys, PAGE_SIZE, page_virt,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS);
	if (err)
		return NULL;

#if PAGE_DIR_LEVELS == 4
	pgd = pgd_offset((pgd_t *)page_virt, virt);
	if (!pgd_valid(pgd))
		return NULL;
	phys = (unsigned long)pud4l_offset(pgd, page_table_offset, 0);
	err = page_map_create(hv_page_table, phys, PAGE_SIZE, page_virt,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS);
	if (err)
		return NULL;

	pud = pud4l_offset((pgd_t *)&page_virt, page_table_offset, virt);
#elif PAGE_DIR_LEVELS == 3
	pud = pud3l_offset((pgd_t *)page_virt, virt);
#else
# error Unsupported paging level
#endif
	if (!pud_valid(pud))
		return NULL;
	phys = (unsigned long)pmd_offset(pud, page_table_offset, 0);
	err = page_map_create(hv_page_table, phys, PAGE_SIZE, page_virt,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS);
	if (err)
		return NULL;

	pmd = pmd_offset((pud_t *)&page_virt, page_table_offset, virt);
	if (!pmd_valid(pmd))
		return NULL;
	if (pmd_is_hugepage(pmd))
		phys = phys_address_hugepage(pmd, virt);
	else {
		phys = (unsigned long)pte_offset(pmd, page_table_offset, 0);
		err = page_map_create(hv_page_table, phys, PAGE_SIZE,
				      page_virt, PAGE_READONLY_FLAGS,
				      PAGE_DEFAULT_FLAGS, PAGE_DIR_LEVELS);
		if (err)
			return NULL;

		pte = pte_offset((pmd_t *)&page_virt, page_table_offset, virt);
		if (!pte_valid(pte))
			return NULL;
		phys = phys_address(pte, 0) + page_table_offset;
	}

	err = page_map_create(hv_page_table, phys, PAGE_SIZE, page_virt,
			      flags, PAGE_DEFAULT_FLAGS, PAGE_DIR_LEVELS);
	if (err)
		return NULL;

	return (void *)page_virt;
}

int paging_init(void)
{
	unsigned long per_cpu_pages, config_pages, bitmap_pages;
	unsigned long n;
	u8 *addr;
	int err;

	mem_pool.pages =
		(hypervisor_header.size - (__page_pool - __start)) / PAGE_SIZE;
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
	for (addr = __start; addr < __start + hypervisor_header.size;
	     addr += PAGE_SIZE) {
		err = page_map_create(hv_page_table, page_map_hvirt2phys(addr),
				      PAGE_SIZE, (unsigned long)addr,
				      PAGE_DEFAULT_FLAGS, PAGE_DEFAULT_FLAGS,
				      PAGE_DIR_LEVELS);
		if (err)
			goto error_nomem;
	}

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
