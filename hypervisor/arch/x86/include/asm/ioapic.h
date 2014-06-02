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

#define IOAPIC_BASE_ADDR	0xfec00000
#define IOAPIC_REG_INDEX	0x00
#define IOAPIC_REG_DATA		0x10
#define IOAPIC_REG_EOI		0x40
#define IOAPIC_ID		0x00
#define IOAPIC_VER		0x01
#define IOAPIC_REDIR_TBL_START	0x10
#define IOAPIC_REDIR_TBL_END	0x3f

int ioapic_init(void);

void ioapic_cell_init(struct cell *cell);
void ioapic_root_cell_shrink(struct jailhouse_cell_desc *config);
void ioapic_cell_exit(struct cell *cell);

int ioapic_access_handler(struct cell *cell, bool is_write, u64 addr,
			  u32 *value);
