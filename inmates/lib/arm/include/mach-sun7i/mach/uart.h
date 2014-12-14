/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define UART_BASE	((void *)0x01c29c00)
#define UART_CLOCK_REG	((void *)0x01c2006c)
#define UART_GATE_NR	23
