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
#include <jailhouse/ivshmem.h>

void arch_ivshmem_trigger_interrupt(struct ivshmem_endpoint *ive,
				    unsigned int vector)
{
}

int arch_ivshmem_update_msix(struct ivshmem_endpoint *ive, unsigned int vector,
			     bool enabled)
{
	return -ENOSYS;
}

void arch_ivshmem_update_intx(struct ivshmem_endpoint *ive, bool enabled)
{
}
