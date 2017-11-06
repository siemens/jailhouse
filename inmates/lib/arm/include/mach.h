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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef CONFIG_MACH_JETSON_TK1
#define CON_TYPE	"8250"
#define CON_BASE	0x70006300 /* UART D on tegra124, exposed to the DB9
				      connector of the Jetson TK1 */
/* Do not enable the clock in the inmate, as enabling the clock requires access
 * to the tegra-car (Clock and Reset Controller) */
#define CON_CLOCK_REG  0
#define CON_GATE_NR    0

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0x50041000)
#define GICC_V2_BASE	((void *)0x50042000)

#elif defined(CONFIG_MACH_BANANAPI)
#define CON_TYPE	"8250"
#define CON_BASE	0x01c29c00
#define CON_DIVIDER	0x0d

#define CON_CLOCK_REG	0x01c2006c
#define CON_GATE_NR	23

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0x01c81000)
#define GICC_V2_BASE	((void *)0x01c82000)

#elif defined(CONFIG_MACH_EMCON_RZG)
#define CON_TYPE	"HSCIF"
#define CON_BASE	0xe6ee0000
#define CON_DIVIDER	0x10

#define CON_CLOCK_REG	0xe615014c
#define CON_GATE_NR	15

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0x01c81000)
#define GICC_V2_BASE	((void *)0x01c82000)

#elif defined(CONFIG_MACH_ORANGEPI0)
#define CON_TYPE	"8250"
#define CON_BASE	0x01c28000

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0x01c81000)
#define GICC_V2_BASE	((void *)0x01c82000)

#elif defined(CONFIG_MACH_VEXPRESS)
#define CON_TYPE	"PL011"
#define CON_BASE	0x1c090000

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0x2c001000)
#define GICC_V2_BASE	((void *)0x2c002000)

/*
#define GIC_VERSION	3
#define GICD_V3_BASE	((void *)0x2f000000)
#define GICR_V3_BASE	((void *)0x2f100000)
*/

#endif

#ifndef TIMER_IRQ
#define TIMER_IRQ      27
#endif
