/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2020
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/entry.h>
#include <jailhouse/pci.h>

u32 arch_pci_read_config(u16 bdf, u16 address, unsigned int size)
{
	return 0;
}

void arch_pci_write_config(u16 bdf, u16 address, u32 value, unsigned int size)
{
}

int arch_pci_add_physical_device(struct cell *cell, struct pci_device *device)
{
	return -ENOSYS;
}

void arch_pci_remove_physical_device(struct pci_device *device)
{
}

void arch_pci_set_suppress_msi(struct pci_device *device,
			       const struct jailhouse_pci_capability *cap,
			       bool suppress)
{
}

int arch_pci_update_msi(struct pci_device *device,
			const struct jailhouse_pci_capability *cap)
{
	return -ENOSYS;
}

int arch_pci_update_msix_vector(struct pci_device *device, unsigned int index)
{
	return -ENOSYS;
}
