/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Henning Schild <henning.schild@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/pci.h>

#include "pci.h"

static void jailhouse_pci_add_device(const struct jailhouse_pci_device *dev)
{
	int num;
	struct pci_bus *bus;

	bus = pci_find_bus(dev->domain, PCI_BUS_NUM(dev->bdf));
	if (bus) {
		num = pci_scan_slot(bus, dev->bdf & 0xff);
		if (num) {
			pci_lock_rescan_remove();
			pci_bus_assign_resources(bus);
			pci_bus_add_devices(bus);
			pci_unlock_rescan_remove();
		}
	}
}

static void jailhouse_pci_remove_device(const struct jailhouse_pci_device *dev)
{
	struct pci_dev *l_dev;

	l_dev = pci_get_bus_and_slot(PCI_BUS_NUM(dev->bdf), dev->bdf & 0xff);
	if (l_dev)
		pci_stop_and_remove_bus_device_locked(l_dev);
}

void jailhouse_pci_do_all_devices(struct cell *cell, unsigned int type,
				  unsigned int action)
{
	unsigned int n;
	const struct jailhouse_pci_device *dev;

	dev = cell->pci_devices;
	for (n = cell->num_pci_devices; n > 0; n--) {
		if (dev->type == type) {
			if (action == JAILHOUSE_PCI_ACTION_ADD)
				jailhouse_pci_add_device(dev);
			else if (action == JAILHOUSE_PCI_ACTION_DEL)
				jailhouse_pci_remove_device(dev);
		}
		dev++;
	}
}

int jailhouse_pci_cell_setup(struct cell *cell,
			     const struct jailhouse_cell_desc *cell_desc)
{
	if (cell_desc->num_pci_devices == 0)
		/* cell is zero-initialized, no need to set pci fields */
		return 0;

	if (cell_desc->num_pci_devices >=
	    ULONG_MAX / sizeof(struct jailhouse_pci_device))
		return -EINVAL;

	cell->num_pci_devices = cell_desc->num_pci_devices;
	cell->pci_devices = vmalloc(sizeof(struct jailhouse_pci_device) *
				    cell->num_pci_devices);
	if (!cell->pci_devices)
		return -ENOMEM;

	memcpy(cell->pci_devices,
	       jailhouse_cell_pci_devices(cell_desc),
	       sizeof(struct jailhouse_pci_device) * cell->num_pci_devices);

	return 0;
}

void jailhouse_pci_cell_cleanup(struct cell *cell)
{
	vfree(cell->pci_devices);
}
