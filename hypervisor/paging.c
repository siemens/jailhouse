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

#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/control.h>

#define BITS_PER_PAGE		(PAGE_SIZE * 8)

#define INVALID_PAGE_NR		(~0UL)

#define PAGE_SCRUB_ON_FREE	0x1

/**
 * Offset between virtual and physical hypervisor addresses.
 *
 * Jailhouse operates in a physically contiguous memory region,
 * enabling offset-based address conversion.
 * @note Private, use page_map_hvirt2phys() or page_map_phys2hvirt() instead.
 */
unsigned long page_offset;

/** Page pool containing physical pages for use by the hypervisor. */
struct page_pool mem_pool;
/** Page pool containing virtual pages for remappings by the hypervisor. */
struct page_pool remap_pool = {
	.base_address = (void *)REMAP_BASE,
	.pages = BITS_PER_PAGE * NUM_REMAP_BITMAP_PAGES,
};

/** Descriptor of the hypervisor paging structures. */
struct paging_structures hv_paging_structs;

/** Descriptor of paging structures used when parking CPUs. */
struct paging_structures parking_pt;

/**
 * Trivial implementation of paging::get_phys (for non-terminal levels)
 * @param pte See paging::get_phys.
 * @param virt See paging::get_phys.
 *
 * @return @c INVALID_PHYS_ADDR.
 *
 * @see paging
 */
unsigned long paging_get_phys_invalid(pt_entry_t pte, unsigned long virt)
{
	return INVALID_PHYS_ADDR;
}

static unsigned long find_next_free_page(struct page_pool *pool,
					 unsigned long start)
{
	unsigned long bmp_pos, bmp_val, page_nr;
	unsigned long start_mask = 0;

	if (start >= pool->pages)
		return INVALID_PAGE_NR;

	/*
	 * If we don't start on the beginning of a bitmap word, create a mask
	 * to mark the pages before the start page as (virtually) used.
	 */
	if (start % BITS_PER_LONG > 0)
		start_mask = ~0UL >> (BITS_PER_LONG - (start % BITS_PER_LONG));

	for (bmp_pos = start / BITS_PER_LONG;
	     bmp_pos < pool->pages / BITS_PER_LONG; bmp_pos++) {
		bmp_val = pool->used_bitmap[bmp_pos] | start_mask;
		start_mask = 0;
		if (bmp_val != ~0UL) {
			page_nr = ffzl(bmp_val) + bmp_pos * BITS_PER_LONG;
			if (page_nr >= pool->pages)
				break;
			return page_nr;
		}
	}

	return INVALID_PAGE_NR;
}

/**
 * Allocate consecutive pages from the specified pool.
 * @param pool		Page pool to allocate from.
 * @param num		Number of pages.
 * @param align_mask	Choose start so that start_page_no & align_mask == 0.
 *
 * @return Pointer to first page or NULL if allocation failed.
 *
 * @see page_free
 */
static void *page_alloc_internal(struct page_pool *pool, unsigned int num,
				 unsigned long align_mask)
{
	unsigned long aligned_start, pool_start, next, start, last;
	unsigned int allocated;

	pool_start = (unsigned long)pool->base_address >> PAGE_SHIFT;

	/* The pool itself might not be aligned as required. */
	aligned_start = ((pool_start + align_mask) & ~align_mask) - pool_start;
	next = aligned_start;

restart:
	/* Forward the search start to the next aligned page. */
	if ((next - aligned_start) & align_mask)
		next += num - ((next - aligned_start) & align_mask);

	start = next = find_next_free_page(pool, next);
	if (start == INVALID_PAGE_NR || num == 0)
		return NULL;

	/* Enforce alignment (none of align_mask is 0). */
	if ((start - aligned_start) & align_mask)
		goto restart;

	for (allocated = 1, last = start; allocated < num;
	     allocated++, last = next) {
		next = find_next_free_page(pool, last + 1);
		if (next == INVALID_PAGE_NR)
			return NULL;
		if (next != last + 1)
			goto restart;	/* not consecutive */
	}

	for (allocated = 0; allocated < num; allocated++)
		set_bit(start + allocated, pool->used_bitmap);

	pool->used_pages += num;

	return pool->base_address + start * PAGE_SIZE;
}

/**
 * Allocate consecutive pages from the specified pool.
 * @param pool	Page pool to allocate from.
 * @param num	Number of pages.
 *
 * @return Pointer to first page or NULL if allocation failed.
 *
 * @see page_free
 */
void *page_alloc(struct page_pool *pool, unsigned int num)
{
	return page_alloc_internal(pool, num, 0);
}

/**
 * Allocate aligned consecutive pages from the specified pool.
 * @param pool	Page pool to allocate from.
 * @param num	Number of pages. Num needs to be a power of 2.
 *
 * @return Pointer to first page or NULL if allocation failed.
 *
 * @see page_free
 */
void *page_alloc_aligned(struct page_pool *pool, unsigned int num)
{
	return page_alloc_internal(pool, num, num - 1);
}

/**
 * Release pages to the specified pool.
 * @param pool	Page pool to release to.
 * @param page	Address of first page.
 * @param num	Number of pages.
 *
 * @see page_alloc
 */
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

/**
 * Translate virtual to physical address according to given paging structures.
 * @param pg_structs	Paging structures to use for translation.
 * @param virt		Virtual address.
 * @param flags		Access flags that have to be supported by the mapping,
 * 			see @ref PAGE_ACCESS_FLAGS.
 *
 * @return Physical address on success or @c INVALID_PHYS_ADDR if the virtual
 * 	   address could not be translated or the requested access is not
 * 	   supported by the mapping.
 *
 * @see paging_phys2hvirt
 * @see paging_hvirt2phys
 * @see arch_paging_gphys2phys
 */
unsigned long paging_virt2phys(const struct paging_structures *pg_structs,
			       unsigned long virt, unsigned long flags)
{
	const struct paging *paging = pg_structs->root_paging;
	page_table_t pt = pg_structs->root_table;
	unsigned long phys;
	pt_entry_t pte;

	while (1) {
		pte = paging->get_entry(pt, virt);
		if (!paging->entry_valid(pte, flags))
			return INVALID_PHYS_ADDR;
		phys = paging->get_phys(pte, virt);
		if (phys != INVALID_PHYS_ADDR)
			return phys;
		pt = paging_phys2hvirt(paging->get_next_pt(pte));
		paging++;
	}
}

static void flush_pt_entry(pt_entry_t pte, unsigned long paging_flags)
{
	if (paging_flags & PAGING_COHERENT)
		arch_paging_flush_cpu_caches(pte, sizeof(*pte));
}

static int split_hugepage(bool hv_paging, const struct paging *paging,
			  pt_entry_t pte, unsigned long virt,
			  unsigned long paging_flags)
{
	unsigned long phys = paging->get_phys(pte, virt);
	struct paging_structures sub_structs;
	unsigned long page_mask, flags;

	if (phys == INVALID_PHYS_ADDR)
		return 0;

	page_mask = ~((unsigned long)paging->page_size - 1);
	phys &= page_mask;
	virt &= page_mask;

	flags = paging->get_flags(pte);

	sub_structs.hv_paging = hv_paging;
	sub_structs.root_paging = paging + 1;
	sub_structs.root_table = page_alloc(&mem_pool, 1);
	if (!sub_structs.root_table)
		return -ENOMEM;
	paging->set_next_pt(pte, paging_hvirt2phys(sub_structs.root_table));
	flush_pt_entry(pte, paging_flags);

	return paging_create(&sub_structs, phys, paging->page_size, virt,
			     flags, paging_flags);
}

/**
 * Create or modify a page map.
 * @param pg_structs	Descriptor of paging structures to be used.
 * @param phys		Physical address of the region to be mapped.
 * @param size		Size of the region.
 * @param virt		Virtual address the region should be mapped to.
 * @param access_flags	Flags describing the permitted page access, see
 * 			@ref PAGE_ACCESS_FLAGS.
 * @param paging_flags	Flags describing the paging mode, see @ref PAGING_FLAGS.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @note The function aims at using the largest possible page size for the
 * mapping but does not consolidate with neighboring mappings.
 *
 * @see paging_destroy
 * @see paging_get_guest_pages
 */
int paging_create(const struct paging_structures *pg_structs,
		  unsigned long phys, unsigned long size, unsigned long virt,
		  unsigned long access_flags, unsigned long paging_flags)
{
	phys &= PAGE_MASK;
	virt &= PAGE_MASK;
	size = PAGE_ALIGN(size);

	while (size > 0) {
		const struct paging *paging = pg_structs->root_paging;
		page_table_t pt = pg_structs->root_table;
		struct paging_structures sub_structs;
		pt_entry_t pte;
		int err;

		while (1) {
			pte = paging->get_entry(pt, virt);
			if (paging->page_size > 0 &&
			    paging->page_size <= size &&
			    ((phys | virt) & (paging->page_size - 1)) == 0 &&
			    (paging_flags & PAGING_HUGE ||
			     paging->page_size == PAGE_SIZE)) {
				/*
				 * We might be overwriting a more fine-grained
				 * mapping, so release it first. This cannot
				 * fail as we are working along hugepage
				 * boundaries.
				 */
				if (paging->page_size > PAGE_SIZE) {
					sub_structs.root_paging = paging;
					sub_structs.root_table = pt;
					sub_structs.hv_paging =
						pg_structs->hv_paging;
					paging_destroy(&sub_structs, virt,
						       paging->page_size,
						       paging_flags);
				}
				paging->set_terminal(pte, phys, access_flags);
				flush_pt_entry(pte, paging_flags);
				break;
			}
			if (paging->entry_valid(pte, PAGE_PRESENT_FLAGS)) {
				err = split_hugepage(pg_structs->hv_paging,
						     paging, pte, virt,
						     paging_flags);
				if (err)
					return err;
				pt = paging_phys2hvirt(
						paging->get_next_pt(pte));
			} else {
				pt = page_alloc(&mem_pool, 1);
				if (!pt)
					return -ENOMEM;
				paging->set_next_pt(pte,
						    paging_hvirt2phys(pt));
				flush_pt_entry(pte, paging_flags);
			}
			paging++;
		}
		if (pg_structs->hv_paging)
			arch_paging_flush_page_tlbs(virt);

		phys += paging->page_size;
		virt += paging->page_size;
		size -= paging->page_size;
	}
	return 0;
}

/**
 * Destroy a page map.
 * @param pg_structs	Descriptor of paging structures to be used.
 * @param virt		Virtual address the region to be unmapped.
 * @param size		Size of the region.
 * @param paging_flags	Flags describing the paging mode, see @ref PAGING_FLAGS.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @note If required, this function tries to break up hugepages if they should
 * be unmapped only partially. This may require allocating additional pages for
 * the paging structures, thus can fail. Unmap request that covers only full
 * pages never fail.
 *
 * @see paging_create
 */
int paging_destroy(const struct paging_structures *pg_structs,
		   unsigned long virt, unsigned long size,
		   unsigned long paging_flags)
{
	size = PAGE_ALIGN(size);

	while (size > 0) {
		const struct paging *paging = pg_structs->root_paging;
		page_table_t pt[MAX_PAGE_TABLE_LEVELS];
		unsigned long page_size;
		pt_entry_t pte;
		int n = 0;
		int err;

		/* walk down the page table, saving intermediate tables */
		pt[0] = pg_structs->root_table;
		while (1) {
			pte = paging->get_entry(pt[n], virt);
			if (!paging->entry_valid(pte, PAGE_PRESENT_FLAGS))
				break;
			if (paging->get_phys(pte, virt) != INVALID_PHYS_ADDR) {
				unsigned long page_start;

				/*
				 * If the region to be unmapped doesn't fully
				 * cover the hugepage, the hugepage will need to
				 * be split.
				 */
				page_size = paging->page_size ?
					paging->page_size : PAGE_SIZE;
				page_start = virt & ~(page_size-1);

				/*
				 * It's possible that virt + size overflows to
				 * exactly 0 (e.g. a 512MB region starting at
				 * 0xe0000000 with 32-bit addresses) during
				 * normal execution. Any overflow beyond that is
				 * a programming error.
				 *
				 * To handle this case, subtract 1 from the size
				 * when comparing both sides. Note that size and
				 * page_size are always > 0, so there's no risk
				 * of underflow.
				 */
				if (virt <= page_start &&
				    virt + (size - 1) >=
				    page_start + (page_size - 1))
					break;

				err = split_hugepage(pg_structs->hv_paging,
						     paging, pte, virt,
						     paging_flags);
				if (err)
					return err;
			}
			pt[++n] = paging_phys2hvirt(paging->get_next_pt(pte));
			paging++;
		}
		/* advance by page size of current level paging */
		page_size = paging->page_size ? paging->page_size : PAGE_SIZE;

		/* walk up again, clearing entries, releasing empty tables */
		while (1) {
			paging->clear_entry(pte);
			flush_pt_entry(pte, paging_flags);
			if (n == 0 || !paging->page_table_empty(pt[n]))
				break;
			page_free(&mem_pool, pt[n], 1);
			paging--;
			pte = paging->get_entry(pt[--n], virt);
		}
		if (pg_structs->hv_paging)
			arch_paging_flush_page_tlbs(virt);

		if (page_size > size)
			break;
		virt += page_size;
		size -= page_size;
	}
	return 0;
}

static unsigned long
paging_gvirt2gphys(const struct guest_paging_structures *pg_structs,
		   unsigned long gvirt, unsigned long tmp_page,
		   unsigned long flags)
{
	unsigned long page_table_gphys = pg_structs->root_table_gphys;
	const struct paging *paging = pg_structs->root_paging;
	unsigned long gphys, phys;
	pt_entry_t pte;
	int err;

	while (1) {
		/* map guest page table */
		phys = arch_paging_gphys2phys(page_table_gphys,
					      PAGE_READONLY_FLAGS);
		if (phys == INVALID_PHYS_ADDR)
			return INVALID_PHYS_ADDR;
		err = paging_create(&this_cpu_data()->pg_structs, phys,
				    PAGE_SIZE, tmp_page, PAGE_READONLY_FLAGS,
				    PAGING_NON_COHERENT | PAGING_NO_HUGE);
		if (err)
			return INVALID_PHYS_ADDR;

		/* evaluate page table entry */
		pte = paging->get_entry((page_table_t)tmp_page, gvirt);
		if (!paging->entry_valid(pte, flags))
			return INVALID_PHYS_ADDR;
		gphys = paging->get_phys(pte, gvirt);
		if (gphys != INVALID_PHYS_ADDR)
			return gphys;
		page_table_gphys = paging->get_next_pt(pte);
		paging++;
	}
}

/**
 * Map physical device resource into hypervisor address space.
 * @param phys		Physical address of the resource.
 * @param size		Size of the resource.
 *
 * @return Virtual mapping address of the resource or NULL on error.
 */
void *paging_map_device(unsigned long phys, unsigned long size)
{
	void *virt;

	virt = page_alloc(&remap_pool, PAGES(size));
	if (!virt)
		return NULL;

	if (paging_create(&hv_paging_structs, phys, size, (unsigned long)virt,
			  PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE,
			  PAGING_NON_COHERENT | PAGING_HUGE) != 0) {
		page_free(&remap_pool, virt, PAGES(size));
		return NULL;
	}

	return virt;
}

/**
 * Unmap physical device resource from hypervisor address space.
 * @param phys		Physical address of the resource.
 * @param virt		Virtual address of the resource.
 * @param size		Size of the resource.
 *
 * @note Unmap must use the same parameters as provided to / returned by
 * paging_map_device().
 */
void paging_unmap_device(unsigned long phys, void *virt, unsigned long size)
{
	/* Cannot fail if paired with paging_map_device. */
	paging_destroy(&hv_paging_structs, (unsigned long)virt, size,
		       PAGING_NON_COHERENT);
	page_free(&remap_pool, virt, PAGES(size));
}

/**
 * Create a top-level link to the common hypervisor page table.
 * @param pg_dest_structs	Descriptor of the target paging structures.
 * @param virt			Virtual start address of the linked region.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @note The link is only created at the lop level of page table. The source
 * needs to point the page table hierarchy, not a terminal entry.
 */
int paging_create_hvpt_link(const struct paging_structures *pg_dest_structs,
			    unsigned long virt)
{
	const struct paging *paging = hv_paging_structs.root_paging;
	pt_entry_t source_pte, dest_pte;

	source_pte = paging->get_entry(hv_paging_structs.root_table, virt);
	dest_pte = paging->get_entry(pg_dest_structs->root_table, virt);

	/*
	 * Source page table must by populated and the to-be-linked
	 * region must not be a terminal entry.
	 */
	if (!paging->entry_valid(source_pte, PAGE_PRESENT_FLAGS) ||
	    paging->get_phys(source_pte, virt) != INVALID_PHYS_ADDR)
		return trace_error(-EINVAL);

	paging->set_next_pt(dest_pte, paging->get_next_pt(source_pte));

	/* Mapping is always non-coherent, so no flush_pt_entry needed. */
	arch_paging_flush_page_tlbs(virt);

	return 0;
}

/**
 * Map guest (cell) pages into the hypervisor address space.
 * @param pg_structs	Descriptor of the guest paging structures if @c gaddr
 * 			is a guest-virtual address or @c NULL if it is a
 * 			guest-physical address.
 * @param gaddr		Guest address of the first page to be mapped.
 * @param num		Number of pages to be mapped.
 * @param flags		Access flags for the hypervisor mapping, see
 * 			@ref PAGE_ACCESS_FLAGS.
 *
 * @return Pointer to first mapped page or @c NULL on error.
 *
 * @note The mapping is done only for the calling CPU and must thus only be
 * used by the very same CPU.
 *
 * @note The mapping is only temporary, valid until the next invocation of
 * page_map_get_guest_pages() on this CPU. It does not require explicit
 * unmapping when it is no longer needed.
 */
void *paging_get_guest_pages(const struct guest_paging_structures *pg_structs,
			     unsigned long gaddr, unsigned int num,
			     unsigned long flags)
{
	unsigned long phys, gphys, page_virt = TEMPORARY_MAPPING_BASE;
	int err;

	if (num > NUM_TEMPORARY_PAGES)
		return NULL;
	while (num-- > 0) {
		if (pg_structs && pg_structs->root_paging)
			gphys = paging_gvirt2gphys(pg_structs, gaddr,
						   page_virt, flags);
		else
			gphys = gaddr;

		phys = arch_paging_gphys2phys(gphys, flags);
		if (phys == INVALID_PHYS_ADDR)
			return NULL;
		/* map guest page */
		err = paging_create(&this_cpu_data()->pg_structs, phys,
				    PAGE_SIZE, page_virt, flags,
				    PAGING_NON_COHERENT | PAGING_NO_HUGE);
		if (err)
			return NULL;
		gaddr += PAGE_SIZE;
		page_virt += PAGE_SIZE;
	}
	return (void *)TEMPORARY_MAPPING_BASE;
}

int paging_map_all_per_cpu(unsigned int cpu, bool enable)
{
	struct per_cpu *cpu_data = per_cpu(cpu);

	/*
	 * Note that the physical address does not matter for !enable because
	 * we mark all pages non-present in that case.
	 */
	return paging_create(&hv_paging_structs, paging_hvirt2phys(cpu_data),
			sizeof(struct per_cpu) - sizeof(struct public_per_cpu),
			(unsigned long)cpu_data,
			enable ? PAGE_DEFAULT_FLAGS : PAGE_NONPRESENT_FLAGS,
			PAGING_NON_COHERENT | PAGING_HUGE);
}

/**
 * Initialize the page mapping subsystem.
 *
 * @return 0 on success, negative error code otherwise.
 */
int paging_init(void)
{
	unsigned long n, per_cpu_pages, config_pages, bitmap_pages;
	unsigned long vaddr, flags;
	int err;

	per_cpu_pages = hypervisor_header.max_cpus *
		sizeof(struct per_cpu) / PAGE_SIZE;

	config_pages = PAGES(jailhouse_system_config_size(system_config));

	page_offset = JAILHOUSE_BASE -
		system_config->hypervisor_memory.phys_start;

	mem_pool.pages = (system_config->hypervisor_memory.size -
		(__page_pool - (u8 *)&hypervisor_header)) / PAGE_SIZE;
	bitmap_pages = (mem_pool.pages + BITS_PER_PAGE - 1) / BITS_PER_PAGE;

	if (mem_pool.pages <= per_cpu_pages + config_pages + bitmap_pages)
		return -ENOMEM;

	mem_pool.base_address = __page_pool;
	mem_pool.used_bitmap =
		(unsigned long *)(__page_pool + per_cpu_pages * PAGE_SIZE +
				  config_pages * PAGE_SIZE);
	mem_pool.used_pages = per_cpu_pages + config_pages + bitmap_pages;
	for (n = 0; n < mem_pool.used_pages; n++)
		set_bit(n, mem_pool.used_bitmap);
	mem_pool.flags = PAGE_SCRUB_ON_FREE;

	remap_pool.used_bitmap = page_alloc(&mem_pool, NUM_REMAP_BITMAP_PAGES);

	hv_paging_structs.hv_paging = true;
	hv_paging_structs.root_table =
		(page_table_t)public_per_cpu(0)->root_table_page;

	arch_paging_init();

	parking_pt.root_table = page_alloc_aligned(&mem_pool,
						   CELL_ROOT_PT_PAGES);
	if (!parking_pt.root_table)
		return -ENOMEM;

	/* Replicate hypervisor mapping of Linux */
	err = paging_create(&hv_paging_structs,
			    paging_hvirt2phys(&hypervisor_header),
			    system_config->hypervisor_memory.size,
			    (unsigned long)&hypervisor_header,
			    PAGE_DEFAULT_FLAGS,
			    PAGING_NON_COHERENT | PAGING_HUGE);
	if (err)
		return err;

	/*
	 * Make sure any permission changes on the per_cpu region can be
	 * performed without allocations of page table pages.
	 */
	for (n = 0; n < hypervisor_header.max_cpus; n++) {
		err = paging_map_all_per_cpu(n, true);
		if (err)
			return err;
	}

	if (CON_IS_MMIO(system_config->debug_console.flags)) {
		vaddr = (unsigned long)hypervisor_header.debug_console_base;
		/* check if console overlaps remapping region */
		if (vaddr + system_config->debug_console.size >= REMAP_BASE &&
		    vaddr < REMAP_BASE + remap_pool.pages * PAGE_SIZE)
			return trace_error(-EINVAL);

		flags = PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE;
		if (system_config->debug_console.type ==
		    JAILHOUSE_CON_TYPE_EFIFB)
			flags = PAGE_DEFAULT_FLAGS | PAGE_FLAG_FRAMEBUFFER;
		err = paging_create(&hv_paging_structs,
				    system_config->debug_console.address,
				    system_config->debug_console.size, vaddr,
				    flags, PAGING_NON_COHERENT | PAGING_HUGE);
		if (err)
			return err;
	}

	return 0;
}

/**
 * Dump usage statistic of the page pools.
 * @param when String that characterizes the associated event.
 */
void paging_dump_stats(const char *when)
{
	printk("Page pool usage %s: mem %ld/%ld, remap %ld/%ld\n", when,
	       mem_pool.used_pages, mem_pool.pages,
	       remap_pool.used_pages, remap_pool.pages);
}
