/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2016-2019
 *
 * Author:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/ivshmem.h>
#include <asm/irqchip.h>

void arch_ivshmem_trigger_interrupt(struct ivshmem_endpoint *ive)
{
	unsigned int irq_id = ive->irq_cache.id;

	if (irq_id)
		irqchip_set_pending(NULL, irq_id);
}

int arch_ivshmem_update_msix(struct pci_device *device, bool enabled)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;
	unsigned int irq_id = 0;

	if (enabled) {
		/* FIXME: validate MSI-X target address */
		irq_id = device->msix_vectors[0].data;
		if (irq_id < 32 || !irqchip_irq_in_cell(device->cell, irq_id))
			return -EPERM;
	}

	/*
	 * Lock used as barrier, ensuring all interrupts triggered after return
	 * use the new setting.
	 */
	spin_lock(&ive->irq_lock);
	ive->irq_cache.id = irq_id;
	spin_unlock(&ive->irq_lock);

	return 0;
}

void arch_ivshmem_update_intx(struct ivshmem_endpoint *ive, bool enabled)
{
	u8 pin = ive->cspace[PCI_CFG_INT/4] >> 8;
	struct pci_device *device = ive->device;

	/*
	 * Lock used as barrier, ensuring all interrupts triggered after return
	 * use the new setting.
	 */
	spin_lock(&ive->irq_lock);
	ive->irq_cache.id = enabled ?
		(32 + device->cell->config->vpci_irq_base + pin - 1) : 0;
	spin_unlock(&ive->irq_lock);
}
