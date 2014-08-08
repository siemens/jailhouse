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

#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <asm/ioapic.h>
#include <asm/spinlock.h>

#include <jailhouse/cell-config.h>

#define IOAPIC_BASE_ADDR	0xfec00000
#define IOAPIC_REG_INDEX	0x00
#define IOAPIC_REG_DATA		0x10
#define IOAPIC_REG_EOI		0x40
#define IOAPIC_ID		0x00
#define IOAPIC_VER		0x01
#define IOAPIC_REDIR_TBL_START	0x10
#define IOAPIC_REDIR_TBL_END	0x3f

static DEFINE_SPINLOCK(ioapic_lock);
static void *ioapic;

static u32 ioapic_reg_read(unsigned int reg)
{
	u32 value;

	spin_lock(&ioapic_lock);

	mmio_write32(ioapic + IOAPIC_REG_INDEX, reg);
	value = mmio_read32(ioapic + IOAPIC_REG_DATA);

	spin_unlock(&ioapic_lock);

	return value;
}

static void ioapic_reg_write(unsigned int reg, u32 value)
{
	spin_lock(&ioapic_lock);

	mmio_write32(ioapic + IOAPIC_REG_INDEX, reg);
	mmio_write32(ioapic + IOAPIC_REG_DATA, value);

	spin_unlock(&ioapic_lock);
}

int ioapic_init(void)
{
	int err;

	ioapic = page_alloc(&remap_pool, 1);
	if (!ioapic)
		return -ENOMEM;
	err = page_map_create(&hv_paging_structs, IOAPIC_BASE_ADDR, PAGE_SIZE,
			      (unsigned long)ioapic,
			      PAGE_DEFAULT_FLAGS | PAGE_FLAG_UNCACHED,
			      PAGE_MAP_NON_COHERENT);
	if (err)
		return err;

	ioapic_cell_init(&root_cell);

	return 0;
}

static const struct jailhouse_irqchip *
ioapic_find_config(struct jailhouse_cell_desc *config)
{
	const struct jailhouse_irqchip *irqchip =
		jailhouse_cell_irqchips(config);
	unsigned int n;

	for (n = 0; n < config->num_irqchips; n++, irqchip++)
		if (irqchip->address == IOAPIC_BASE_ADDR)
			return irqchip;
	return NULL;
}

void ioapic_cell_init(struct cell *cell)
{
	const struct jailhouse_irqchip *irqchip =
		ioapic_find_config(cell->config);

	if (irqchip) {
		cell->ioapic_pin_bitmap = irqchip->pin_bitmap;
		if (cell != &root_cell)
			root_cell.ioapic_pin_bitmap &= ~irqchip->pin_bitmap;
	}
}

void ioapic_cell_exit(struct cell *cell)
{
	const struct jailhouse_irqchip *cell_irqchip =
		ioapic_find_config(cell->config);
	const struct jailhouse_irqchip *root_irqchip =
		ioapic_find_config(root_cell.config);

	if (cell_irqchip && root_irqchip)
		root_cell.ioapic_pin_bitmap |= cell_irqchip->pin_bitmap &
			root_irqchip->pin_bitmap;
}

/**
 * x86_ioapic_handler() - Handler for accesses to IOAPIC
 * @cell:	Request issuing cell
 * @is_write:	True if write access
 * @addr:	Address accessed
 * @value:	Pointer to value for reading/writing
 *
 * Return: 1 if handled successfully, 0 if unhandled, -1 on access error
 */
int ioapic_access_handler(struct cell *cell, bool is_write, u64 addr,
			  u32 *value)
{
	u32 index, entry;

	if (addr < IOAPIC_BASE_ADDR || addr >= IOAPIC_BASE_ADDR + PAGE_SIZE)
		return 0;

	switch (addr - IOAPIC_BASE_ADDR) {
	case IOAPIC_REG_INDEX:
		if (is_write)
			cell->ioapic_index_reg_val = *value;
		else
			*value = cell->ioapic_index_reg_val;
		return 1;
	case IOAPIC_REG_DATA:
		index = cell->ioapic_index_reg_val;

		if (index == IOAPIC_ID || index == IOAPIC_VER) {
			if (is_write)
				goto invalid_access;
			*value = ioapic_reg_read(index);
			return 1;
		}

		if (index < IOAPIC_REDIR_TBL_START ||
		    index > IOAPIC_REDIR_TBL_END)
			goto invalid_access;

		entry = (index - IOAPIC_REDIR_TBL_START) / 2;
		/* Note: we only support one IOAPIC per system */
		if ((cell->ioapic_pin_bitmap & (1UL << entry)) == 0)
			goto invalid_access;

		if (is_write)
			ioapic_reg_write(index, *value);
		else
			*value = ioapic_reg_read(index);
		return 1;
	case IOAPIC_REG_EOI:
		if (!is_write)
			goto invalid_access;
		// TODO: virtualize
		mmio_write32(ioapic + IOAPIC_REG_EOI, *value);
		return 1;
	}

invalid_access:
	panic_printk("FATAL: Invalid IOAPIC %s, reg: %x, index: %x\n",
		     is_write ? "write" : "read", addr - IOAPIC_BASE_ADDR,
		     cell->ioapic_index_reg_val);
	return -1;
}
