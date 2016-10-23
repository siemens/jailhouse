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

#define UART_BASE	((void *)0x70006300)

/* Do not enable the clock in the inmate, as enabling the clock requires access
 * to the tegra-car (Clock and Reset Controller) */
#define UART_CLOCK_REG  ((void *)0)
#define UART_GATE_NR    0
