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

#define IOAPIC_NUM_PINS		24

union ioapic_redir_entry {
	struct {
		u8 vector;
		u8 delivery_mode:3;
		u8 dest_logical:1;
		u8 delivery_status:1;
		u8 pin_polarity:1;
		u8 remote_irr:1;
		u8 level_triggered:1;
		u32 mask:1;
		u32 reserved:31;
		u8 edid;
		u8 destination;
	} __attribute__((packed)) native;
	struct {
		u8 vector;
		u8 zero:3;
		u8 int_index15:1;
		u8 delivery_status:1;
		u8 pin_polarity:1;
		u8 remote_irr:1;
		u8 level_triggered:1;
		u32 mask:1;
		u32 reserved:31;
		u16 remapped:1;
		u16 int_index:15;
	} __attribute__((packed)) remap;
	u32 raw[2];
} __attribute__((packed));

int ioapic_init(void);
void ioapic_prepare_handover(void);

int ioapic_cell_init(struct cell *cell);
void ioapic_cell_exit(struct cell *cell);

void ioapic_config_commit(struct cell *cell_added_removed);

int ioapic_access_handler(struct cell *cell, bool is_write, u64 addr,
			  u32 *value);

void ioapic_shutdown(void);
