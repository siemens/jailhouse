/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_CELL_H
#define _JAILHOUSE_CELL_H

#include <jailhouse/paging.h>
#include <jailhouse/pci.h>
#include <asm/cell.h>

#include <jailhouse/cell-config.h>
#include <jailhouse/hypercall.h>

/** Cell-related states. */
struct cell {
	union {
		/** Communication region. */
		struct jailhouse_comm_region comm_region;
		/** Padding to full page size. */
		u8 padding[PAGE_SIZE];
	} __attribute__((aligned(PAGE_SIZE))) comm_page;
	/**< Page containing the communication region (shared with cell). */

	/** Architecture-specific fields. */
	struct arch_cell arch;

	/** ID of the cell. */
	unsigned int id;
	/** Number of pages used for storing cell-specific states and
	 * configuration data. */
	unsigned int data_pages;
	/** Pointer to static cell description. */
	struct jailhouse_cell_desc *config;

	/** Pointer to cell's CPU set. */
	struct cpu_set *cpu_set;
	/** Stores the cell's CPU set if small enough. */
	struct cpu_set small_cpu_set;

	/** True while the cell can be loaded by the root cell. */
	bool loadable;

	/** Pointer to next cell in the system. */
	struct cell *next;

	/** List of PCI devices assigned to this cell. */
	struct pci_device *pci_devices;
	/** List of PCI devices assigned to this cell that support MSI-X. */
	struct pci_device *msix_device_list;
	/** List of virtual PCI devices assigned to this cell. */
	struct pci_device *virtual_device_list;
};

extern struct cell root_cell;

#endif /* !_JAILHOUSE_CELL_H */
