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
#include <linux/vmalloc.h>

#include "pci.h"

static int jailhouse_pci_stub_probe(struct pci_dev *dev,
				    const struct pci_device_id *id)
{
	dev_info(&dev->dev, "claimed for use in non-root cell\n");
	return 0;
}

/**
 * When assigning PCI devices to cells we need to make sure Linux in the
 * root-cell does not use them anymore. As long as devices are assigned to
 * other cells the root-cell must not access the devices.
 * Unfortunately we can not just use PCI hotplugging to remove/re-add
 * devices at runtime. Linux will reprogram the BARs and locate ressources
 * where we do not expect/allow them.
 * So Jailhouse acts as a PCI dummy driver and claims the devices while
 * other cells use them. When creating a cell devices will be unbound from
 * their drivers and bound to jailhouse. When a cell is destroyed jailhouse
 * will release its devices. When jailhouse is disabled it will release all
 * assigned devices.
 * @see jailhouse_pci_claim_release
 *
 * When releasing devices they will not be bound to any driver anymore and
 * from Linuxs point of view the jailhouse dummy will still look like a
 * valid driver. Assignment back to the original driver has to be done
 * manually.
 */
static struct pci_driver jailhouse_pci_stub_driver = {
	.name		= "jailhouse-pci-stub",
	.id_table	= NULL,
	.probe		= jailhouse_pci_stub_probe,
};

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

static void jailhouse_pci_claim_release(const struct jailhouse_pci_device *dev,
					unsigned int action)
{
	int err;
	struct pci_bus *bus;
	struct pci_dev *l_dev;
	struct device_driver *drv;

	bus = pci_find_bus(dev->domain, PCI_BUS_NUM(dev->bdf));
	if (!bus)
		return;
	l_dev = pci_get_slot(bus, dev->bdf & 0xff);
	drv = l_dev->dev.driver;

	if (action == JAILHOUSE_PCI_ACTION_CLAIM) {
		if (drv == &jailhouse_pci_stub_driver.driver)
			return;
		device_release_driver(&l_dev->dev);
		err = pci_add_dynid(&jailhouse_pci_stub_driver, l_dev->vendor,
				    l_dev->device, l_dev->subsystem_vendor,
				    l_dev->subsystem_device, l_dev->class,
				    0, 0);
		if (err)
			dev_warn(&l_dev->dev, "failed to add dynamic id (%d)\n",
			         err);
	} else {
		/* on "jailhouse disable" we will come here with the
		 * request to release all pci devices, so check the driver */
		if (drv == &jailhouse_pci_stub_driver.driver)
			device_release_driver(&l_dev->dev);
	}
}

/**
 * Register jailhouse as a PCI device driver so it can claim assigned devices.
 *
 * @return 0 on success, or error code
 */
int jailhouse_pci_register(void)
{
	return pci_register_driver(&jailhouse_pci_stub_driver);
}

/**
 * Unegister jailhouse as a PCI device driver.
 */
void jailhouse_pci_unregister(void)
{
	pci_unregister_driver(&jailhouse_pci_stub_driver);
}

/**
 * Apply the given action to all of the cells PCI devices matching the given
 * type.
 * @param cell		the cell containing the PCI devices
 * @param type		PCI device type (JAILHOUSE_PCI_TYPE_*)
 * @param action	action (JAILHOUSE_PCI_ACTION_*)
 */
void jailhouse_pci_do_all_devices(struct cell *cell, unsigned int type,
				  unsigned int action)
{
	unsigned int n;
	const struct jailhouse_pci_device *dev;

	dev = cell->pci_devices;
	for (n = cell->num_pci_devices; n > 0; n--) {
		if (dev->type == type) {
			switch(action) {
			case JAILHOUSE_PCI_ACTION_ADD:
				jailhouse_pci_add_device(dev);
				break;
			case JAILHOUSE_PCI_ACTION_DEL:
				jailhouse_pci_remove_device(dev);
				break;
			default:
				jailhouse_pci_claim_release(dev, action);
			}
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
