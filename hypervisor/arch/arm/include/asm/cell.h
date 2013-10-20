/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_CELL_H
#define _JAILHOUSE_ASM_CELL_H

#include <asm/types.h>
#include <asm/paging.h>

#include <jailhouse/cell-config.h>

struct cell {
	char name[JAILHOUSE_CELL_NAME_MAXLEN+1];

	struct cpu_set *cpu_set;
	struct cpu_set small_cpu_set;

	unsigned long page_offset;

	struct cell *next;
};

extern struct cell *cell_list;

#endif /* !_JAILHOUSE_ASM_CELL_H */
