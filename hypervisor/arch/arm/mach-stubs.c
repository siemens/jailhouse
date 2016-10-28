/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/mach.h>

const unsigned int __attribute__((weak)) mach_mmio_regions;

int __attribute__((weak)) mach_init(void)
{
	return 0;
}

void __attribute__((weak)) mach_cell_init(struct cell *cell)
{
}

void __attribute__((weak)) mach_cell_exit(struct cell *cell)
{
}
