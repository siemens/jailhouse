/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_PLATFORM_H
#define _JAILHOUSE_ASM_PLATFORM_H

#ifndef __ASSEMBLY__

#ifdef CONFIG_ARCH_VEXPRESS

#define UART_BASE_PHYS	((void *)0x1c090000)
#define UART_BASE_VIRT	((void *)0xf8090000)

#endif /* CONFIG_ARCH_VEXPRESS */
#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_PLATFORM_H */
