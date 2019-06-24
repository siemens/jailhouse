/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/pci.h>

u32 arch_pci_read_config(u16 bdf, u16 address, unsigned int size)
{
	return -1;
}

void arch_pci_write_config(u16 bdf, u16 address, u32 value, unsigned int size)
{
}

int arch_pci_add_physical_device(struct cell *cell, struct pci_device *device)
{
	return 0;
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
	const struct jailhouse_pci_device *info = device->info;
	unsigned int n;

	/*
	 * NOTE: We don't have interrupt remapping yet. So we write the values
	 * the cell passed without modifications. Probably not safe on all
	 * platforms.
	 */
	for (n = 1; n < (info->msi_64bits ? 4 : 3); n++)
		pci_write_config(info->bdf, cap->start + n * 4,
				 device->msi_registers.raw[n], 4);

	return 0;
}

int arch_pci_update_msix_vector(struct pci_device *device, unsigned int index)
{
	/* NOTE: See arch_pci_update_msi. */
	mmio_write64_split(&device->msix_table[index].address,
			   device->msix_vectors[index].address);
	mmio_write32(&device->msix_table[index].data,
		     device->msix_vectors[index].data);
	return 0;
}
