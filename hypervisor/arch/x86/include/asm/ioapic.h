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

#include <jailhouse/cell.h>
#include <asm/spinlock.h>

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

/**
 * Global physical IOAPIC irqchip state.
 */
struct phys_ioapic {
	/** Physical address to identify the instance. */
	unsigned long base_addr;
	/** Virtual address of mapped registers. */
	void *reg_base;
	/** Lock protecting physical accesses. */
	spinlock_t lock;
	/** Shadow state of redirection entries as seen by the cells. */
	union ioapic_redir_entry shadow_redir_table[IOAPIC_NUM_PINS];
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
	/** Bitmap of pins currently assigned to this cell. Only requires 32
	 * bits because the IOAPIC has just 24 pins. */
	u32 pin_bitmap;
};

static inline unsigned int ioapic_mmio_count_regions(struct cell *cell)
{
	return cell->config->num_irqchips;
}

int ioapic_init(void);
void ioapic_prepare_handover(void);

int ioapic_cell_init(struct cell *cell);
void ioapic_cell_exit(struct cell *cell);

void ioapic_config_commit(struct cell *cell_added_removed);

void ioapic_shutdown(void);
