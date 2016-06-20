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

#ifndef _JAILHOUSE_ASM_IVSHMEM_H
#define _JAILHOUSE_ASM_IVSHMEM_H

#include <asm/apic.h>

struct arch_pci_ivshmem {
	struct apic_irq_message irq_msg;
};

#endif /* !_JAILHOUSE_ASM_IVSHMEM_H */
