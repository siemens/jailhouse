/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2020
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/paging.h>

void arch_paging_init(void)
{
}

// Might be misplaced
unsigned long arch_paging_gphys2phys(unsigned long gphys, unsigned long flags)
{
	return 0;
}
