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

#ifndef _JAILHOUSE_ASM_PAGING_H
#define _JAILHOUSE_ASM_PAGING_H

#include <jailhouse/types.h>
#include <jailhouse/utils.h>
#include <asm/processor.h>
#include <asm/sysregs.h>

#define PAGE_SHIFT		12
#define PAGE_SIZE		(1 << PAGE_SHIFT)
#define PAGE_MASK		~(PAGE_SIZE - 1)
#define PAGE_OFFS_MASK		(PAGE_SIZE - 1)

#define MAX_PAGE_TABLE_LEVELS	3

/*
 * When T0SZ == 0 and SL0 == 0, the EL2 MMU starts the IPA->PA translation at
 * the level 2 table. The second table is indexed by IPA[31:21], the third one
 * by IPA[20:12].
 * This would allows to cover a 4GB memory map by using 4 concatenated level-2
 * page tables and thus provide better table walk performances.
 * For the moment, the core doesn't allow to use concatenated pages, so we will
 * use three levels instead, starting at level 1.
 *
 * TODO: add a "u32 concatenated" field to the paging struct
 */
#if MAX_PAGE_TABLE_LEVELS < 3
#define T0SZ			0
#define SL0			0
#define PADDR_OFF		(14 - T0SZ)
#define L2_VADDR_MASK		BIT_MASK(21, 17 + PADDR_OFF)
#else
#define T0SZ			0
#define SL0			1
#define PADDR_OFF		(5 - T0SZ)
#define L1_VADDR_MASK		BIT_MASK(26 + PADDR_OFF, 30)
#define L2_VADDR_MASK		BIT_MASK(29, 21)
#endif

#define L3_VADDR_MASK		BIT_MASK(20, 12)

/*
 * Stage-1 and Stage-2 lower attributes.
 * FIXME: The upper attributes (contiguous hint and XN) are not currently in
 * use. If needed in the future, they should be shifted towards the lower word,
 * since the core uses unsigned long to pass the flags.
 * An arch-specific typedef for the flags as well as the addresses would be
 * useful.
 * The contiguous bit is a hint that allows the PE to store blocks of 16 pages
 * in the TLB. This may be a useful optimisation.
 */
#define PTE_ACCESS_FLAG		(0x1 << 10)
/*
 * When combining shareability attributes, the stage-1 ones prevail. So we can
 * safely leave everything non-shareable at stage 2.
 */
#define PTE_NON_SHAREABLE	(0x0 << 8)
#define PTE_OUTER_SHAREABLE	(0x2 << 8)
#define PTE_INNER_SHAREABLE	(0x3 << 8)

#define PTE_MEMATTR(val)	((val) << 2)
#define PTE_FLAG_TERMINAL	(0x1 << 1)
#define PTE_FLAG_VALID		(0x1 << 0)

/* These bits differ in stage 1 and 2 translations */
#define S1_PTE_NG		(0x1 << 11)
#define S1_PTE_ACCESS_RW	(0x0 << 7)
#define S1_PTE_ACCESS_RO	(0x1 << 7)
/* Res1 for EL2 stage-1 tables */
#define S1_PTE_ACCESS_EL0	(0x1 << 6)

#define S2_PTE_ACCESS_RO	(0x1 << 6)
#define S2_PTE_ACCESS_WO	(0x2 << 6)
#define S2_PTE_ACCESS_RW	(0x3 << 6)

/*
 * Descriptor pointing to a page table
 * (only for L1 and L2. L3 uses this encoding for terminal entries...)
 */
#define PTE_TABLE_FLAGS		0x3

#define PTE_L1_BLOCK_ADDR_MASK	BIT_MASK(39, 30)
#define PTE_L2_BLOCK_ADDR_MASK	BIT_MASK(39, 21)
#define PTE_TABLE_ADDR_MASK	BIT_MASK(39, 12)
#define PTE_PAGE_ADDR_MASK	BIT_MASK(39, 12)

#define BLOCK_1G_VADDR_MASK	BIT_MASK(29, 0)
#define BLOCK_2M_VADDR_MASK	BIT_MASK(20, 0)

#define TTBR_MASK		BIT_MASK(47, PADDR_OFF)
#define VTTBR_VMID_SHIFT	48

#define HTCR_RES1		((1 << 31) | (1 << 23))
#define VTCR_RES1		((1 << 31))
#define TCR_RGN_NON_CACHEABLE	0x0
#define TCR_RGN_WB_WA		0x1
#define TCR_RGN_WT		0x2
#define TCR_RGN_WB		0x3
#define TCR_NON_SHAREABLE	0x0
#define TCR_OUTER_SHAREABLE	0x2
#define TCR_INNER_SHAREABLE	0x3

#define TCR_SH0_SHIFT		12
#define TCR_ORGN0_SHIFT		10
#define TCR_IRGN0_SHIFT		8
#define TCR_SL0_SHIFT		6
#define TCR_S_SHIFT		4

/*
 * Hypervisor memory attribute indexes:
 *   0: normal WB, RA, WA, non-transient
 *   1: device
 *   2: normal non-cacheable
 *   3-7: unused
 */
#define DEFAULT_HMAIR0		0x004404ff
#define DEFAULT_HMAIR1		0x00000000
#define HMAIR_IDX_WBRAWA	0
#define HMAIR_IDX_DEV		1
#define HMAIR_IDX_NC		2

/* Stage 2 memory attributes (MemAttr[3:0]) */
#define S2_MEMATTR_OWBIWB	0xf
#define S2_MEMATTR_DEV		0x1

#define S1_PTE_FLAG_NORMAL	PTE_MEMATTR(HMAIR_IDX_WBRAWA)
#define S1_PTE_FLAG_DEVICE	PTE_MEMATTR(HMAIR_IDX_DEV)
#define S1_PTE_FLAG_UNCACHED	PTE_MEMATTR(HMAIR_IDX_NC)

#define S2_PTE_FLAG_NORMAL	PTE_MEMATTR(S2_MEMATTR_OWBIWB)
#define S2_PTE_FLAG_DEVICE	PTE_MEMATTR(S2_MEMATTR_DEV)

#define S1_DEFAULT_FLAGS	(PTE_FLAG_VALID | PTE_ACCESS_FLAG	\
				| S1_PTE_FLAG_NORMAL | PTE_INNER_SHAREABLE\
				| S1_PTE_ACCESS_EL0)

/* Macros used by the core, only for the EL2 stage-1 mappings */
#define PAGE_FLAG_DEVICE	S1_PTE_FLAG_DEVICE
#define PAGE_DEFAULT_FLAGS	(S1_DEFAULT_FLAGS | S1_PTE_ACCESS_RW)
#define PAGE_READONLY_FLAGS	(S1_DEFAULT_FLAGS | S1_PTE_ACCESS_RO)
#define PAGE_PRESENT_FLAGS	PTE_FLAG_VALID
#define PAGE_NONPRESENT_FLAGS	0

#define INVALID_PHYS_ADDR	(~0UL)

#define REMAP_BASE		0x00100000UL
#define NUM_REMAP_BITMAP_PAGES	1

#define NUM_TEMPORARY_PAGES	16

#ifndef __ASSEMBLY__

typedef u64 *pt_entry_t;

/* Only executed on hypervisor paging struct changes */
static inline void arch_paging_flush_page_tlbs(unsigned long page_addr)
{
	/*
	 * This instruction is UNDEF at EL1, but the whole TLB is invalidated
	 * before enabling the EL2 stage 1 MMU anyway.
	 */
	if (is_el2())
		arm_write_sysreg(TLBIMVAH, page_addr & PAGE_MASK);
}

extern unsigned int cache_line_size;

/* Used to clean the PAGE_MAP_COHERENT page table changes */
static inline void arch_paging_flush_cpu_caches(void *addr, long size)
{
	do {
		/* Clean & invalidate by MVA to PoC */
		arm_write_sysreg(DCCIMVAC, addr);
		size -= cache_line_size;
		addr += cache_line_size;
	} while (size > 0);
}

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PAGING_H */
