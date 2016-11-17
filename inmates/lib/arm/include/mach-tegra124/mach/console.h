/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define CON_TYPE	"8250"

#define CON_BASE	0x70006300 /* UART D on tegra124, exposed to the DB9
				      connector of the Jetson TK1 */

/* Do not enable the clock in the inmate, as enabling the clock requires access
 * to the tegra-car (Clock and Reset Controller) */
#define CON_CLOCK_REG  0
#define CON_GATE_NR    0
