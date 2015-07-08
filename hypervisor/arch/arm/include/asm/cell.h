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

#ifndef _JAILHOUSE_ASM_CELL_H
#define _JAILHOUSE_ASM_CELL_H

#include <jailhouse/types.h>
#include <asm/smp.h>
#include <asm/spinlock.h>

#ifndef __ASSEMBLY__

#include <jailhouse/cell-config.h>
#include <jailhouse/paging.h>
#include <jailhouse/hypercall.h>

/** ARM-specific cell states. */
struct arch_cell {
	struct paging_structures mm;
	struct smp_ops *smp;

	spinlock_t caches_lock;
	bool needs_flush;

	u64 spis;

	unsigned int last_virt_id;
};

/** PCI-related cell states. */
struct pci_cell {
};

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_CELL_H */
