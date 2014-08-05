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

#include <asm/cell.h>

int ioapic_init(void);

void ioapic_cell_init(struct cell *cell);
void ioapic_cell_exit(struct cell *cell);

int ioapic_access_handler(struct cell *cell, bool is_write, u64 addr,
			  u32 *value);
