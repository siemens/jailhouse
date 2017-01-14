/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2015-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifdef CONFIG_MACH_JETSON_TK1
#define CON_TYPE	"8250"
#define CON_BASE	0x70006300 /* UART D on tegra124, exposed to the DB9
				      connector of the Jetson TK1 */
/* Do not enable the clock in the inmate, as enabling the clock requires access
 * to the tegra-car (Clock and Reset Controller) */
#define CON_CLOCK_REG  0
#define CON_GATE_NR    0

#define GICD_V2_BASE	((void *)0x50041000)
#define GICC_V2_BASE	((void *)0x50042000)

#define TIMER_IRQ	27

#elif defined(CONFIG_MACH_BANANAPI)
#define CON_TYPE	"8250"
#define CON_BASE	0x01c29c00
#define CON_DIVIDER	0x0d

#define CON_CLOCK_REG	0x01c2006c
#define CON_GATE_NR	23

#define GICD_V2_BASE	((void *)0x01c81000)
#define GICC_V2_BASE	((void *)0x01c82000)

#define TIMER_IRQ	27

#elif defined(CONFIG_MACH_ORANGEPI0)
#define CON_TYPE	"8250"
#define CON_BASE	0x01c28000

#define GICD_V2_BASE	((void *)0x01c81000)
#define GICC_V2_BASE	((void *)0x01c82000)

#define TIMER_IRQ	27

#elif defined(CONFIG_MACH_VEXPRESS)
#define CON_TYPE	"PL011"
#define CON_BASE	0x1c090000

#define GICD_V2_BASE	((void *)0x2c001000)
#define GICC_V2_BASE	((void *)0x2c002000)

#define GICD_V3_BASE	((void *)0x2f000000)
#define GICR_V3_BASE	((void *)0x2f100000)

#define TIMER_IRQ	27

#endif
