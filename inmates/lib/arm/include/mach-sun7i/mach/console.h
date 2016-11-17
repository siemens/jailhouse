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

#define CON_TYPE	"8250"

#define CON_BASE	0x01c29c00
#define CON_CLOCK_REG	0x01c2006c
#define CON_GATE_NR	23
