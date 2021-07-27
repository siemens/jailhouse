/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015-2016 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_PAGING_H
#define _JAILHOUSE_ASM_PAGING_H

#include <jailhouse/types.h>
#include <jailhouse/utils.h>
#include <asm/dcaches.h>
#include <asm/processor.h>
#include <asm/sysregs.h>

/*
 * This file is based on hypervisor/arch/arm/include/asm/paging.h for AArch32.
 * However, there are some differences. AArch64 supports different granule
 * sizes for pages (4Kb, 16Kb, and 64Kb), while AArch32 supports only a 4Kb
 * native page size. AArch64 also supports 4 levels of page tables, numbered
 * L0-3, while AArch32 supports only 3 levels numbered L1-3.
 *
 * We currently only implement 4Kb granule size for the page tables.
 * We support physical address ranges of up to 48 bits.
 */

#define PAGE_SHIFT		12

#define MAX_PAGE_TABLE_LEVELS	4

#define L0_VADDR_MASK		BIT_MASK(47, 39)
#define L1_VADDR_MASK		BIT_MASK(38, 30)
#define L2_VADDR_MASK		BIT_MASK(29, 21)

#define L3_VADDR_MASK		BIT_MASK(20, 12)

/*
 * Stage-1 and Stage-2 lower attributes.
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

#define PTE_L0_BLOCK_ADDR_MASK	BIT_MASK(47, 39)
#define PTE_L1_BLOCK_ADDR_MASK	BIT_MASK(47, 30)
#define PTE_L2_BLOCK_ADDR_MASK	BIT_MASK(47, 21)
#define PTE_TABLE_ADDR_MASK	BIT_MASK(47, 12)
#define PTE_PAGE_ADDR_MASK	BIT_MASK(47, 12)

#define BLOCK_512G_VADDR_MASK	BIT_MASK(38, 0)
#define BLOCK_1G_VADDR_MASK	BIT_MASK(29, 0)
#define BLOCK_2M_VADDR_MASK	BIT_MASK(20, 0)

/*
 * AARCH64_TODO: the way TTBR_MASK is handled is almost certainly wrong. The
 * low bits of the TTBR should be zero, however this is an alignment requirement
 * as well for the actual location of the page table root. We get around the
 * buggy behaviour in the AArch32 code we share, by setting the mask to the
 * de facto alignment employed by the arch independent code: one page.
 */
#define TTBR_MASK		BIT_MASK(47, 12)
#define VTTBR_VMID_SHIFT	48

#define TCR_EL2_RES1		((1 << 31) | (1 << 23))
#define VTCR_RES1		((1 << 31))
#define T0SZ(parange)		(64 - parange)
#define SL0_L0			2
#define SL0_L1			1
#define SL0_L2			0
#define PARANGE_48B		0x5
#define TCR_RGN_NON_CACHEABLE	0x0
#define TCR_RGN_WB_WA		0x1
#define TCR_RGN_WT		0x2
#define TCR_RGN_WB		0x3
#define TCR_NON_SHAREABLE	0x0
#define TCR_OUTER_SHAREABLE	0x2
#define TCR_INNER_SHAREABLE	0x3

#define TCR_PS_SHIFT		16
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
#define DEFAULT_MAIR_EL2	0x00000000004404ff
#define MAIR_IDX_WBRAWA		0
#define MAIR_IDX_DEV		1
#define MAIR_IDX_NC		2

/* Stage 2 memory attributes (MemAttr[3:0]) */
#define S2_MEMATTR_OWBIWB	0xf
#define S2_MEMATTR_DEV		0x1

#define S1_PTE_FLAG_NORMAL	PTE_MEMATTR(MAIR_IDX_WBRAWA)
#define S1_PTE_FLAG_DEVICE	PTE_MEMATTR(MAIR_IDX_DEV)
#define S1_PTE_FLAG_UNCACHED	PTE_MEMATTR(MAIR_IDX_NC)

#define S2_PTE_FLAG_NORMAL	PTE_MEMATTR(S2_MEMATTR_OWBIWB)
#define S2_PTE_FLAG_DEVICE	PTE_MEMATTR(S2_MEMATTR_DEV)

#define S1_DEFAULT_FLAGS	(PTE_FLAG_VALID | PTE_ACCESS_FLAG	\
				| S1_PTE_FLAG_NORMAL | PTE_INNER_SHAREABLE\
				| S1_PTE_ACCESS_EL0)

/* Memory Model Feature Register 0 */
#define ID_AA64MMFR0_PARANGE_SHIFT	0

/* Macros used by the core, only for the EL2 stage-1 mappings */
#define PAGE_FLAG_FRAMEBUFFER	S1_PTE_FLAG_DEVICE
#define PAGE_FLAG_DEVICE	S1_PTE_FLAG_DEVICE
#define PAGE_DEFAULT_FLAGS	(S1_DEFAULT_FLAGS | S1_PTE_ACCESS_RW)
#define PAGE_READONLY_FLAGS	(S1_DEFAULT_FLAGS | S1_PTE_ACCESS_RO)
#define PAGE_PRESENT_FLAGS	PTE_FLAG_VALID
#define PAGE_NONPRESENT_FLAGS	0

#define INVALID_PHYS_ADDR	(~0UL)

#define UART_BASE		0xffffc0000000

/**
 * Location of per-CPU temporary mapping region in hypervisor address space.
 */
#define TEMPORARY_MAPPING_BASE	0xff0000000000UL
#define NUM_TEMPORARY_PAGES	16

#define REMAP_BASE		0xff8000000000UL
#define NUM_REMAP_BITMAP_PAGES	4

#ifndef __ASSEMBLY__

struct cell;
struct paging_structures;

typedef u64 *pt_entry_t;

extern unsigned int cpu_parange, cpu_parange_encoded;

unsigned int get_cpu_parange(void);

/* The size of the cpu_parange, determines from which level we can
 * start from the S2 translations, and the size of the first level
 * page table */
#define T0SZ_CELL		T0SZ(cpu_parange)
#define SL0_CELL		((cpu_parange >= 44) ? SL0_L0 : SL0_L1)
#define CELL_ROOT_PT_PAGES				\
	({ unsigned int ret = 1;			\
	   if (cpu_parange > 39 && cpu_parange < 44)	\
		ret = 1 << (cpu_parange - 39);		\
	   ret; })

/* Just match the host's PARange */
#define VTCR_CELL		(T0SZ_CELL | (SL0_CELL << TCR_SL0_SHIFT)\
				| (TCR_RGN_WB_WA << TCR_IRGN0_SHIFT)	\
				| (TCR_RGN_WB_WA << TCR_ORGN0_SHIFT)	\
				| (TCR_INNER_SHAREABLE << TCR_SH0_SHIFT)\
				| (cpu_parange_encoded << TCR_PS_SHIFT)	\
				| VTCR_RES1)

int arm_paging_cell_init(struct cell *cell);
void arm_paging_cell_destroy(struct cell *cell);

void arm_paging_vcpu_init(struct paging_structures *pg_structs);

static inline void arm_paging_vcpu_flush_tlbs(void)
{
	/*
	 * Invalidate all stage-1 and 2 TLB entries for the current VMID
	 * ERET will ensure completion of these ops
	 */
	asm volatile("tlbi vmalls12e1is");
}

/* Only executed on hypervisor paging struct changes */
static inline void arch_paging_flush_page_tlbs(unsigned long page_addr)
{
	asm volatile(
		"dsb ish\n\t"
		"tlbi vae2, %0\n\t"
		"dsb ish\n\t"
		"isb\n\t"
		: : "r" (page_addr >> PAGE_SHIFT));
}

/* Used to clean the PAGE_MAP_COHERENT page table changes */
static inline void arch_paging_flush_cpu_caches(void *addr, long size)
{
	arm_dcaches_flush(addr, size, DCACHE_CLEAN);
}

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PAGING_H */
