/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/cell.h>
#include <asm/spinlock.h>

/*
 * There can be up to 240 pins according to the specs, but it remains unclear
 * how those can be addressed because only an 8-bit index register is specified
 * so far. Therefore, we limit ourselves to implementations with up to 120 pins.
 * That has the additional advantage that we can continue to use only a single
 * struct jailhouse_irqchip to describe an IOAPIC so that we can keep a 1:1
 * relationship with struct cell_ioapic.
 */
#define IOAPIC_MAX_PINS		120

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

/**
 * Global physical IOAPIC irqchip state.
 */
struct phys_ioapic {
	/** Physical address to identify the instance. */
	unsigned long base_addr;
	/** Virtual address of mapped registers. */
	void *reg_base;
	/** Number of supported pins. */
	unsigned int pins;
	/** Lock protecting physical accesses. */
	spinlock_t lock;
	/** Shadow state of redirection entries as seen by the cells. */
	union ioapic_redir_entry shadow_redir_table[IOAPIC_MAX_PINS];
};

/**
 * Per-cell IOAPIC irqchip state.
 */
struct cell_ioapic {
	/** Reference to static irqchip configuration. */
	const struct jailhouse_irqchip *info;
	/** Cell owning at least one pin of the IOAPIC. */
	struct cell *cell;
	/** Reference to corresponding physical IOAPIC */
	struct phys_ioapic *phys_ioapic;

	/** Shadow value of index register. */
	u32 index_reg_val;
	/** Bitmap of pins currently assigned to this cell. */
	u32 pin_bitmap[(IOAPIC_MAX_PINS + 31) / 32];
};

void ioapic_prepare_handover(void);

int ioapic_get_or_add_phys(const struct jailhouse_irqchip *irqchip,
			   struct phys_ioapic **phys_ioapic_ptr);

void ioapic_cell_reset(struct cell *cell);

void ioapic_config_commit(struct cell *cell_added_removed);
