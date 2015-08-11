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

#include <asm/cat.h>

int cat_init(void)
{
	return 0;
}

void cat_update(void)
{
}

int cat_cell_init(struct cell *cell)
{
	return 0;
}

void cat_cell_exit(struct cell *cell)
{
}
