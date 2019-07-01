/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2019
 *
 * Author:
 *  Henning Schild <henning.schild@siemens.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/cell.h>
#include <jailhouse/ivshmem.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <asm/pci.h>

void arch_ivshmem_trigger_interrupt(struct ivshmem_endpoint *ive,
				    unsigned int vector)
{
	if (ive->irq_cache.msg[vector].valid)
		apic_send_irq(ive->irq_cache.msg[vector]);
}

int arch_ivshmem_update_msix(struct ivshmem_endpoint *ive, unsigned int vector,
			     bool enabled)
{
	struct apic_irq_message irq_msg = { .valid = 0 };
	struct pci_device *device = ive->device;
	union x86_msi_vector msi;

	if (enabled) {
		msi.raw.address = device->msix_vectors[vector].address;
		msi.raw.data = device->msix_vectors[vector].data;

		irq_msg = x86_pci_translate_msi(device, vector, 0, msi);

		if (irq_msg.valid &&
		    !apic_filter_irq_dest(device->cell, &irq_msg)) {
			panic_printk("FATAL: ivshmem MSI-X target outside of "
				     "cell \"%s\" device %02x:%02x.%x\n",
				     device->cell->config->name,
				     PCI_BDF_PARAMS(device->info->bdf));
			return -EPERM;
		}
	}

	/*
	 * Lock used as barrier, ensuring all interrupts triggered after return
	 * use the new setting.
	 */
	spin_lock(&ive->irq_lock);
	ive->irq_cache.msg[vector] = irq_msg;
	spin_unlock(&ive->irq_lock);

	return 0;
}

void arch_ivshmem_update_intx(struct ivshmem_endpoint *ive, bool enabled)
{
}
