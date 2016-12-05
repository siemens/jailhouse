/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2016
 *
 * Author:
 *  Henning Schild <henning.schild@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/cell.h>
#include <jailhouse/ivshmem.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <asm/pci.h>

void arch_ivshmem_trigger_interrupt(struct ivshmem_endpoint *ive)
{
	/* Get a copy of the struct before using it. */
	struct apic_irq_message irq_msg = ive->arch.irq_msg;

	/* The read barrier makes sure the copy is consistent. */
	memory_load_barrier();
	if (irq_msg.valid)
		apic_send_irq(irq_msg);
}

int arch_ivshmem_update_msix(struct pci_device *device)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;
	union x86_msi_vector msi = {
		.raw.address = device->msix_vectors[0].address,
		.raw.data = device->msix_vectors[0].data,
	};
	struct apic_irq_message irq_msg;

	/* before doing anything mark the cached irq_msg as invalid,
	 * on success it will be valid on return. */
	ive->arch.irq_msg.valid = 0;
	memory_barrier();

	if (ivshmem_is_msix_masked(ive))
		return 0;

	irq_msg = x86_pci_translate_msi(device, 0, 0, msi);
	if (!irq_msg.valid)
		return 0;

	if (!apic_filter_irq_dest(device->cell, &irq_msg)) {
		panic_printk("FATAL: ivshmem MSI-X target outside of "
			     "cell \"%s\" device %02x:%02x.%x\n",
			     device->cell->config->name,
			     PCI_BDF_PARAMS(device->info->bdf));
		return -EPERM;
	}
	/* now copy the whole struct into our cache and mark the cache
	 * valid at the end */
	irq_msg.valid = 0;
	ive->arch.irq_msg = irq_msg;
	memory_barrier();
	ive->arch.irq_msg.valid = 1;

	return 0;
}

void arch_ivshmem_update_intx(struct ivshmem_endpoint *ive)
{
}
