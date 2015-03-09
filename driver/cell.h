/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_DRIVER_CELL_H
#define _JAILHOUSE_DRIVER_CELL_H

#include <linux/cpumask.h>
#include <linux/list.h>
#include <linux/kobject.h>

#include <jailhouse/cell-config.h>

struct cell {
	struct kobject kobj;
	struct list_head entry;
	unsigned int id;
	cpumask_t cpus_assigned;
	u32 num_memory_regions;
	struct jailhouse_memory *memory_regions;
#ifdef CONFIG_PCI
	u32 num_pci_devices;
	struct jailhouse_pci_device *pci_devices;
#endif /* CONFIG_PCI */
};

#endif /* !_JAILHOUSE_DRIVER_CELL_H */
