/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_CELL_H
#define _JAILHOUSE_ASM_CELL_H

#include <jailhouse/paging.h>

struct cell_ioapic;

/** x86-specific cell states. */
struct arch_cell {
	/** Buffer for the EPT/NPT root-level page table. */
	u8 __attribute__((aligned(PAGE_SIZE))) root_table_page[PAGE_SIZE];

	bool pio_i8042_allowed;

	/* Intel: PIO access bitmap.
	 * AMD: I/O Permissions Map. */
	u8 *io_bitmap;
	union {
		struct {
			/** Paging structures used for cell CPUs. */
			struct paging_structures ept_structs;
		} vmx; /**< Intel VMX-specific fields. */
		struct {
			/** Paging structures used for cell CPUs and IOMMU. */
			struct paging_structures npt_iommu_structs;
		} svm; /**< AMD SVM-specific fields. */
	};

	union {
		struct {
			/** Paging structures used for DMA requests. */
			struct paging_structures pg_structs;
			/** True if interrupt remapping support is emulated for this
			 * cell. */
			bool ir_emulation;
		} vtd; /**< Intel VT-d specific fields. */
	};

	/** Shadow value of PCI config space address port register. */
	u32 pci_addr_port_val;

	/** List of IOAPICs assigned to this cell. */
	struct cell_ioapic *ioapics;
	/** Number of assigned IOAPICs. */
	unsigned int num_ioapics;

	/** Class Of Service for cache allocation (Intel only). */
	u32 cos;
	/** Allocated L3 cache region (Intel only). */
	u64 cat_mask;
};

#endif /* !_JAILHOUSE_ASM_CELL_H */
