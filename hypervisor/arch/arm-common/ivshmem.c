/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2016
 *
 * Author:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/ivshmem.h>

void arch_ivshmem_write_doorbell(struct ivshmem_endpoint *ive)
{
}

int arch_ivshmem_update_msix(struct pci_device *device)
{
	return 0;
}
