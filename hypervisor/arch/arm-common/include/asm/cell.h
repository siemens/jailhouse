/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_CELL_H
#define _JAILHOUSE_ASM_CELL_H

#include <jailhouse/paging.h>

struct pvu_tlb_entry;

struct arch_cell {
	struct paging_structures mm;

	u32 irq_bitmap[1024/32];

	struct {
		u8 ent_count;
		struct pvu_tlb_entry *entries;
	} iommu_pvu; /**< ARM PVU specific fields. */
};

#endif /* !_JAILHOUSE_ASM_CELL_H */
