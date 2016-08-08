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

#ifndef __ASSEMBLY__

#include <jailhouse/paging.h>

/** ARM-specific cell states. */
struct arch_cell {
	struct paging_structures mm;

	u32 irq_bitmap[1024/32];

	unsigned int last_virt_id;
};

/** PCI-related cell states. */
struct pci_cell {
};

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_CELL_H */
