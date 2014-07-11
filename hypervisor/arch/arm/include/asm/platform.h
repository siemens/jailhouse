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

/*
 * All those things are defined in the device tree. This header *must*
 * disappear. The GIC includes will need to be sanitized in order to avoid ID
 * naming conflicts.
 */
#ifndef __ASSEMBLY__

#ifdef CONFIG_ARCH_VEXPRESS

# define UART_BASE_PHYS	((void *)0x1c090000)
# define UART_BASE_VIRT	((void *)0xf8090000)

# ifdef CONFIG_ARM_GIC_V3
#  define GICD_BASE	((void *)0x2f000000)
#  define GICD_SIZE	0x10000
#  define GICR_BASE	((void *)0x2f100000)
#  define GICR_SIZE	0x100000

#  include <asm/gic_v3.h>
# endif /* GIC */

#endif /* CONFIG_ARCH_VEXPRESS */
#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_PLATFORM_H */
