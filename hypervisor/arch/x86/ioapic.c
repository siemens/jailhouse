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

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <asm/apic.h>
#include <asm/ioapic.h>
#include <asm/iommu.h>
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
# define IOAPIC_REDIR_MASK	(1 << 16)

enum ioapic_handover {PINS_ACTIVE, PINS_MASKED};

static DEFINE_SPINLOCK(ioapic_lock);
static void *ioapic;
static union ioapic_redir_entry shadow_redir_table[IOAPIC_NUM_PINS];

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

static struct apic_irq_message
ioapic_translate_redir_entry(struct cell *cell, unsigned int pin,
			     union ioapic_redir_entry entry)
{
	struct apic_irq_message irq_msg = { .valid = 0 };
	unsigned int idx;

	if (iommu_cell_emulates_ir(cell)) {
		if (!entry.remap.remapped)
			return irq_msg;

		idx = entry.remap.int_index | (entry.remap.int_index15 << 15);

		return iommu_get_remapped_root_int(root_cell.ioapic_iommu,
						   root_cell.ioapic_id, pin,
						   idx);
	}

	irq_msg.vector = entry.native.vector;
	irq_msg.delivery_mode = entry.native.delivery_mode;
	irq_msg.level_triggered = entry.native.level_triggered;
	irq_msg.dest_logical = entry.native.dest_logical;
	/* align redir_hint and dest_logical - required by iommu_map_interrupt */
	irq_msg.redir_hint = irq_msg.dest_logical;
	irq_msg.valid = 1;
	irq_msg.destination = entry.native.destination;

	return irq_msg;
}

static int ioapic_virt_redir_write(struct cell *cell, unsigned int reg,
				   u32 value)
{
	unsigned int pin = (reg - IOAPIC_REDIR_TBL_START) / 2;
	struct apic_irq_message irq_msg;
	union ioapic_redir_entry entry;
	int result;

	entry = shadow_redir_table[pin];
	entry.raw[reg & 1] = value;
	shadow_redir_table[pin] = entry;

	/* Do not map the interrupt while masked. */
	if (entry.native.mask) {
		/*
		 * The mask is part of the lower 32 bits. Apply it when that
		 * register half is written.
		 */
		if ((reg & 1) == 0)
			ioapic_reg_write(reg, IOAPIC_REDIR_MASK);
		return 0;
	}

	irq_msg = ioapic_translate_redir_entry(cell, pin, entry);

	result = iommu_map_interrupt(cell, cell->ioapic_id, pin, irq_msg);
	// HACK for QEMU
	if (result == -ENOSYS) {
		/*
		 * Upper 32 bits aren't written when the register is masked.
		 * Write them unconditionally when unmasking to keep an entry
		 * in the consistent state.
		 */
		ioapic_reg_write(reg | 1, entry.raw[1]);
		ioapic_reg_write(reg, entry.raw[reg & 1]);
		return 0;
	}
	if (result < 0)
		return result;

	entry.remap.zero = 0;
	entry.remap.int_index15 = result >> 15;
	entry.remap.remapped = 1;
	entry.remap.int_index = result;
	ioapic_reg_write(reg, entry.raw[reg & 1]);

	return 0;
}

static void ioapic_mask_pins(struct cell *cell, u64 pin_bitmap,
			     enum ioapic_handover handover)
{
	struct apic_irq_message irq_msg;
	union ioapic_redir_entry entry;
	unsigned int pin, reg;

	for (pin = 0; pin < IOAPIC_NUM_PINS; pin++) {
		if (!(pin_bitmap & (1UL << pin)))
			continue;

		reg = IOAPIC_REDIR_TBL_START + pin * 2;

		entry.raw[0] = ioapic_reg_read(reg);
		if (entry.remap.mask)
			continue;

		ioapic_reg_write(reg, IOAPIC_REDIR_MASK);

		if (handover == PINS_MASKED) {
			shadow_redir_table[pin].native.mask = 1;
		} else if (!entry.native.level_triggered) {
			/*
			 * Inject edge-triggered interrupts to avoid losing
			 * events while masked. Linux can handle rare spurious
			 * interrupts.
			 */
			entry = shadow_redir_table[pin];
			irq_msg = ioapic_translate_redir_entry(cell, pin,
							       entry);
			if (irq_msg.valid)
				apic_send_irq(irq_msg);
		}
	}
}

int ioapic_init(void)
{
	unsigned int index;
	void *ioapic_page;
	int err;

	ioapic_page = page_alloc(&remap_pool, 1);
	if (!ioapic_page)
		return -ENOMEM;
	err = paging_create(&hv_paging_structs, IOAPIC_BASE_ADDR, PAGE_SIZE,
			    (unsigned long)ioapic_page,
			    PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE,
			    PAGING_NON_COHERENT);
	if (err)
		return err;
	ioapic = ioapic_page;

	ioapic_cell_init(&root_cell);

	for (index = 0; index < IOAPIC_NUM_PINS * 2; index++)
		shadow_redir_table[index / 2].raw[index % 2] =
			ioapic_reg_read(IOAPIC_REDIR_TBL_START + index);

	ioapic_prepare_handover();

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

void ioapic_prepare_handover(void)
{
	const struct jailhouse_irqchip *irqchip =
		ioapic_find_config(root_cell.config);
	u64 pin_bitmap = 0;

	if (!ioapic)
		return;
	if (irqchip) {
		pin_bitmap = irqchip->pin_bitmap;
		ioapic_mask_pins(&root_cell, pin_bitmap, PINS_ACTIVE);
	}
	ioapic_mask_pins(&root_cell, ~pin_bitmap, PINS_MASKED);
}

void ioapic_cell_init(struct cell *cell)
{
	const struct jailhouse_irqchip *irqchip =
		ioapic_find_config(cell->config);

	if (irqchip) {
		cell->ioapic_id = (u16)irqchip->id;
		cell->ioapic_iommu = (u8)(irqchip->id >> 16);
		cell->ioapic_pin_bitmap = irqchip->pin_bitmap;

		if (cell != &root_cell) {
			root_cell.ioapic_pin_bitmap &= ~irqchip->pin_bitmap;
			ioapic_mask_pins(cell, irqchip->pin_bitmap,
					 PINS_MASKED);
		}
	}
}

void ioapic_cell_exit(struct cell *cell)
{
	const struct jailhouse_irqchip *cell_irqchip =
		ioapic_find_config(cell->config);
	const struct jailhouse_irqchip *root_irqchip =
		ioapic_find_config(root_cell.config);

	if (!cell_irqchip)
		return;

	ioapic_mask_pins(cell, cell_irqchip->pin_bitmap, PINS_MASKED);
	if (root_irqchip)
		root_cell.ioapic_pin_bitmap |= cell_irqchip->pin_bitmap &
			root_irqchip->pin_bitmap;
}

void ioapic_config_commit(struct cell *cell_added_removed)
{
	const struct jailhouse_irqchip *irqchip =
		ioapic_find_config(root_cell.config);
	union ioapic_redir_entry entry;
	unsigned int pin, reg;

	if (!irqchip || !cell_added_removed)
		return;

	for (pin = 0; pin < IOAPIC_NUM_PINS; pin++) {
		if (!(root_cell.ioapic_pin_bitmap & (1UL << pin)))
			continue;

		entry = shadow_redir_table[pin];
		reg = IOAPIC_REDIR_TBL_START + pin * 2;

		/* write high word first to preserve mask initially */
		if (ioapic_virt_redir_write(&root_cell, reg + 1,
					    entry.raw[1]) < 0 ||
		    ioapic_virt_redir_write(&root_cell, reg,
					    entry.raw[0]) < 0) {
			panic_printk("FATAL: Unsupported IOAPIC state, "
				     "pin %d\n", pin);
			panic_stop();
		}
	}
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

		if (is_write) {
			if (ioapic_virt_redir_write(cell, index, *value) < 0)
				goto invalid_access;
		} else {
			index -= IOAPIC_REDIR_TBL_START;
			*value = shadow_redir_table[index / 2].raw[index % 2];
		}
		return 1;
	case IOAPIC_REG_EOI:
		if (!is_write || cell->ioapic_pin_bitmap == 0)
			goto invalid_access;
		/*
		 * Just write the EOI if the cell has any assigned pin. It
		 * would be complex to virtualize it in a way that cells are
		 * unable to ack vectors of other cells. It is therefore not
		 * recommended to use level-triggered IOAPIC interrupts in
		 * non-root cells.
		 */
		mmio_write32(ioapic + IOAPIC_REG_EOI, *value);
		return 1;
	}

invalid_access:
	panic_printk("FATAL: Invalid IOAPIC %s, reg: %x, index: %x\n",
		     is_write ? "write" : "read", addr - IOAPIC_BASE_ADDR,
		     cell->ioapic_index_reg_val);
	return -1;
}

void ioapic_shutdown(void)
{
	int index;

	if (!ioapic)
		return;
	/* write in reverse order to preserve the mask as long as needed */
	for (index = IOAPIC_NUM_PINS * 2 - 1; index >= 0; index--)
		ioapic_reg_write(IOAPIC_REDIR_TBL_START + index,
			shadow_redir_table[index / 2].raw[index % 2]);
}
