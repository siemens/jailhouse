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

# ifdef CONFIG_ARM_GIC_V3
#  define GICD_BASE	((void *)0x2f000000)
#  define GICD_SIZE	0x10000
#  define GICR_BASE	((void *)0x2f100000)
#  define GICR_SIZE	0x100000

#  include <asm/gic_v3.h>
# else /* GICv2 */
#  define GICD_BASE	((void *)0x2c001000)
#  define GICD_SIZE	0x1000
#  define GICC_BASE	((void *)0x2c002000)
/*
 * WARN: most device trees are broken and report only one page for the GICC.
 * It will brake the handle_irq code, since the GICC_DIR register is located at
 * offset 0x1000...
 */
#  define GICC_SIZE	0x2000
#  define GICH_BASE	((void *)0x2c004000)
#  define GICH_SIZE	0x2000
#  define GICV_BASE	((void *)0x2c006000)
#  define GICV_SIZE	0x2000

#  include <asm/gic_v2.h>
# endif /* GIC */

# define MAINTENANCE_IRQ 25
# define SYSREGS_BASE	0x1c010000

#endif /* CONFIG_ARCH_VEXPRESS */

#define HOTPLUG_SPIN	1
/*
#define HOTPLUG_PSCI	1
*/

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_PLATFORM_H */
