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

#ifndef _JAILHOUSE_PAGING_H
#define _JAILHOUSE_PAGING_H

#include <jailhouse/entry.h>
#include <jailhouse/types.h>
#include <asm/paging.h>

/**
 * @defgroup Paging Page Management Subsystem
 *
 * The page management subsystem provides services for allocating physical
 * pages for the hypervisor and reserving remapping pages in the remapping
 * area. It further offers functions to map pages for the hypervisor or cells,
 * walk existing mappings or destroy them again.
 *
 * @{
 */

/** Align address to page boundary (round up). */
#define PAGE_ALIGN(s)		(((s) + PAGE_SIZE-1) & PAGE_MASK)
/** Count number of pages for given size (round up). */
#define PAGES(s)		(((s) + PAGE_SIZE-1) / PAGE_SIZE)

/**
 * Location of per-CPU temporary mapping region in hypervisor address space.
 */
#define TEMPORARY_MAPPING_BASE	REMAP_BASE

/** Page pool state. */
struct page_pool {
	/** Base address of the pool. */
	void *base_address;
	/** Number of managed pages. */
	unsigned long pages;
	/** Number of currently used pages. */
	unsigned long used_pages;
	/** Bitmap of used pages. */
	unsigned long *used_bitmap;
	/** Set @c PAGE_SCRUB_ON_FREE to zero-out pages on release. */
	unsigned long flags;
};

/** Define coherency of page creation/destruction. */
enum paging_coherent {
	/** Make changes visible to non-snooping readers,
	 * i.e. commit them to RAM. */
	PAGING_COHERENT,
	/** Do not force changes into RAM, i.e. avoid costly cache flushes. */
	PAGING_NON_COHERENT,
};

/** Page table reference. */
typedef pt_entry_t page_table_t;

/**
 * Parameters and callbacks for creating and parsing paging structures of a
 * specific level.
 */
struct paging {
	/** Page size of terminal entries in this level or 0 if none are
	 * supported. */
	unsigned int page_size;

	/**
	 * Get entry in given table corresponding to virt address.
	 * @param page_table Reference to page table.
	 * @param virt Virtual address to look up.
	 *
	 * @return Page table entry.
	 */
	pt_entry_t (*get_entry)(page_table_t page_table, unsigned long virt);

	/**
	 * Returns true if entry is a valid and supports the provided access
	 * flags (terminal and non-terminal entries).
	 * @param pte Reference to page table entry.
	 * @param flags Access flags to validate, see @ref PAGE_FLAGS.
	 *
	 * @return True if entry is valid.
	 */
	bool (*entry_valid)(pt_entry_t pte, unsigned long flags);

	/**
	 * Set terminal entry to physical address and access flags.
	 * @param pte Reference to page table entry.
	 * @param phys Target physical address.
	 * @param flags Flags of permitted access, see @ref PAGE_FLAGS.
	 */
	void (*set_terminal)(pt_entry_t pte, unsigned long phys,
			     unsigned long flags);
	/**
	 * Extract physical address from given entry.
	 * @param pte Reference to page table entry.
	 * @param virt Virtual address to look up.
	 *
	 * @return Physical address or @c INVALID_PHYS_ADDR if entry it not
	 * 	   terminal.
	 */
	unsigned long (*get_phys)(pt_entry_t pte, unsigned long virt);
	/**
	 * Extract access flags from given entry.
	 * @param pte Reference to page table entry.
	 *
	 * @return Access flags, see @ref PAGE_FLAGS.
	 *
	 * @note Only valid for terminal entries.
	 */
	unsigned long (*get_flags)(pt_entry_t pte);

	/**
	 * Set entry to physical address of next-level page table.
	 * @param pte Reference to page table entry.
	 * @param next_pt Reference to page table of next level.
	 */
	void (*set_next_pt)(pt_entry_t pte, unsigned long next_pt);
	/**
	 * Get physical address of next-level page table from entry.
	 * @param pte Reference to page table entry.
	 *
	 * @return Physical address.
	 *
	 * @note Only valid for non-terminal entries.
	 */
	unsigned long (*get_next_pt)(pt_entry_t pte);

	/**
	 * Invalidate entry.
	 * @param pte Reference to page table entry.
	 */
	void (*clear_entry)(pt_entry_t pte);

	/**
	 * Returns true if given page table contains no valid entries.
	 * @param page_table Reference to page table.
	 *
	 * @return True if table is empty.
	 */
	bool (*page_table_empty)(page_table_t page_table);
};

/** Describes the root of hierarchical paging structures. */
struct paging_structures {
	/** Pointer to array of paging parameters and callbacks, first element
	 * describing the root level, NULL if paging is disabled. */
	const struct paging *root_paging;
	/** Reference to root-level page table, ignored if root_paging is NULL.
	 */
	page_table_t root_table;
};

/**
 * Describes the root of hierarchical paging structures managed by a guest
 * (cell).
 */
struct guest_paging_structures {
	/** Pointer to array of paging parameters and callbacks, first element
	 * describing the root level. */
	const struct paging *root_paging;
	/** Guest-physical address of the root-level page table. */
	unsigned long root_table_gphys;
};

#include <asm/paging_modes.h>

extern unsigned long page_offset;

extern struct page_pool mem_pool;
extern struct page_pool remap_pool;

extern struct paging_structures hv_paging_structs;
extern struct paging_structures parking_pt;

unsigned long paging_get_phys_invalid(pt_entry_t pte, unsigned long virt);

void *page_alloc(struct page_pool *pool, unsigned int num);
void *page_alloc_aligned(struct page_pool *pool, unsigned int num);
void page_free(struct page_pool *pool, void *first_page, unsigned int num);

/**
 * Translate virtual hypervisor address to physical address.
 * @param hvirt		Virtual address in hypervisor address space.
 *
 * @return Corresponding physical address.
 *
 * @see paging_phys2hvirt
 * @see paging_virt2phys
 * @see arch_paging_gphys2phys
 */
static inline unsigned long paging_hvirt2phys(const volatile void *hvirt)
{
	return (unsigned long)hvirt - page_offset;
}

/**
 * Translate physical address to virtual hypervisor address.
 * @param phys		Physical address to translate.
 *
 * @return Corresponding virtual address in hypervisor address space.
 *
 * @see paging_hvirt2phys
 * @see paging_virt2phys
 * @see arch_paging_gphys2phys
 */
static inline void *paging_phys2hvirt(unsigned long phys)
{
	return (void *)phys + page_offset;
}

unsigned long paging_virt2phys(const struct paging_structures *pg_structs,
			       unsigned long virt, unsigned long flags);

/**
 * Translate guest-physical (cell) address into host-physical address.
 * @param gphys		Guest-physical address to translate.
 * @param flags		Access flags to validate during the translation.
 *
 * @return Corresponding physical address or @c INVALID_PHYS_ADDR if the
 * 	   guest-physical address could not be translated or the requested
 * 	   access is not supported by the mapping.
 *
 * @see paging_phys2hvirt
 * @see paging_hvirt2phys
 * @see paging_virt2phys
 */
unsigned long arch_paging_gphys2phys(unsigned long gphys, unsigned long flags);

int paging_create(const struct paging_structures *pg_structs,
		  unsigned long phys, unsigned long size, unsigned long virt,
		  unsigned long flags, enum paging_coherent coherent);
int paging_destroy(const struct paging_structures *pg_structs,
		   unsigned long virt, unsigned long size,
		   enum paging_coherent coherent);

void *paging_map_device(unsigned long phys, unsigned long size);
void paging_unmap_device(unsigned long phys, void *virt, unsigned long size);

int paging_create_hvpt_link(const struct paging_structures *pg_dest_structs,
			    unsigned long virt);

void *paging_get_guest_pages(const struct guest_paging_structures *pg_structs,
			     unsigned long gaddr, unsigned int num,
			     unsigned long flags);

int paging_init(void);

/**
 * Perform architecture-specific initialization of the page management
 * subsystem.
 */
void arch_paging_init(void);

void paging_dump_stats(const char *when);

/* --- To be provided by asm/paging.h --- */

/**
 * @def PAGE_SIZE
 * Size of the smallest page the architecture supports.
 *
 * @def PAGE_MASK
 * Use for zeroing-out the page offset bits (of the smallest page).
 *
 * @def PAGE_OFFS_MASK
 * Use for zeroing-out the page number bits (of the smallest page).
 */

/**
 * @def MAX_PAGE_TABLE_LEVELS
 * Maximum number of page table levels of the architecture for the hypervisor
 * address space.
 */

/**
 * @defgroup PAGE_FLAGS Page Access flags
 * @{
 *
 * @def PAGE_DEFAULT_FLAGS
 * Page access flags for full access.
 *
 * @def PAGE_READONLY_FLAGS
 * Page access flags for read-only access.
 *
 * @def PAGE_PRESENT_FLAGS
 * Page access flags that indicate presence of the page (unspecified access).
 *
 * @def PAGE_NONPRESENT_FLAGS
 * Page access flags for a non-present page.
 *
 * @}
 */

/**
 * @def INVALID_PHYS_ADDR
 * Physical address value that represents an invalid address, thus is never
 * used by the architecture.
 */

/**
 * @def REMAP_BASE
 * Start address of remapping region in the hypervisor address space.
 *
 * @def NUM_REMAP_BITMAP_PAGES
 * Number of pages for the used-pages bitmap which records allocations in the
 * remapping region.
 */

/**
 * @def NUM_TEMPORARY_PAGES
 * Number of pages used for each CPU in the temporary remapping region.
 */

/**
 * @typedef pt_entry_t
 * Page table entry reference.
 */

/**
 * @fn void arch_paging_flush_page_tlbs(unsigned long page_addr)
 * Flush TLBs related to the specified page.
 * @param page_addr Virtual page address.
 *
 * @see arch_paging_flush_cpu_caches
 */

/**
 * @fn void arch_paging_flush_cpu_caches(void *addr, long size)
 * Flush caches related to the specified region.
 * @param addr Pointer to the region to flush.
 * @param size Size of the region.
 *
 * @see arch_paging_flush_page_tlbs
 */

/** @} */
#endif /* !_JAILHOUSE_PAGING_H */
