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
	struct {
		/* should be first as it requires page alignment */
		u8 __attribute__((aligned(PAGE_SIZE))) io_bitmap[2*PAGE_SIZE];
		pgd_t *ept;
	} vmx;

	struct {
		pgd_t *page_table;
	} vtd;

	char name[JAILHOUSE_CELL_NAME_MAXLEN+1];
	unsigned int id;

	struct cpu_set *cpu_set;
	struct cpu_set small_cpu_set;

	unsigned long page_offset;

	struct cell *next;
};

extern struct cell linux_cell;

#endif /* !_JAILHOUSE_ASM_CELL_H */
