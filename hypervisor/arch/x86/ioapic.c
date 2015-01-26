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

#define IOAPIC_MAX_CHIPS	1

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

#define for_each_phys_ioapic(ioapic, counter)			\
	for ((ioapic) = &phys_ioapics[0], (counter) = 0;	\
	     (counter) < num_phys_ioapics;			\
	     (ioapic)++, (counter)++)

static struct phys_ioapic phys_ioapics[IOAPIC_MAX_CHIPS];
static unsigned int num_phys_ioapics;

static u32 ioapic_reg_read(struct phys_ioapic *ioapic, unsigned int reg)
{
	u32 value;

	spin_lock(&ioapic->lock);

	mmio_write32(ioapic->reg_base + IOAPIC_REG_INDEX, reg);
	value = mmio_read32(ioapic->reg_base + IOAPIC_REG_DATA);

	spin_unlock(&ioapic->lock);

	return value;
}

static void ioapic_reg_write(struct phys_ioapic *ioapic, unsigned int reg,
			     u32 value)
{
	spin_lock(&ioapic->lock);

	mmio_write32(ioapic->reg_base + IOAPIC_REG_INDEX, reg);
	mmio_write32(ioapic->reg_base + IOAPIC_REG_DATA, value);

	spin_unlock(&ioapic->lock);
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
	struct phys_ioapic *phys_ioapic = &phys_ioapics[0];
	struct apic_irq_message irq_msg;
	union ioapic_redir_entry entry;
	int result;

	entry = phys_ioapic->shadow_redir_table[pin];
	entry.raw[reg & 1] = value;
	phys_ioapic->shadow_redir_table[pin] = entry;

	/* Do not map the interrupt while masked. */
	if (entry.native.mask) {
		/*
		 * The mask is part of the lower 32 bits. Apply it when that
		 * register half is written.
		 */
		if ((reg & 1) == 0)
			ioapic_reg_write(phys_ioapic, reg, IOAPIC_REDIR_MASK);
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
		ioapic_reg_write(phys_ioapic, reg | 1, entry.raw[1]);
		ioapic_reg_write(phys_ioapic, reg, entry.raw[reg & 1]);
		return 0;
	}
	if (result < 0)
		return result;

	entry.remap.zero = 0;
	entry.remap.int_index15 = result >> 15;
	entry.remap.remapped = 1;
	entry.remap.int_index = result;
	ioapic_reg_write(phys_ioapic, reg, entry.raw[reg & 1]);

	return 0;
}

static void ioapic_mask_pins(struct cell *cell, u64 pin_bitmap,
			     enum ioapic_handover handover)
{
	struct phys_ioapic *phys_ioapic = &phys_ioapics[0];
	struct apic_irq_message irq_msg;
	union ioapic_redir_entry entry;
	unsigned int pin, reg;

	for (pin = 0; pin < IOAPIC_NUM_PINS; pin++) {
		if (!(pin_bitmap & (1UL << pin)))
			continue;

		reg = IOAPIC_REDIR_TBL_START + pin * 2;

		entry.raw[0] = ioapic_reg_read(phys_ioapic, reg);
		if (entry.remap.mask)
			continue;

		ioapic_reg_write(phys_ioapic, reg, IOAPIC_REDIR_MASK);

		if (handover == PINS_MASKED) {
			phys_ioapic->shadow_redir_table[pin].native.mask = 1;
		} else if (!entry.native.level_triggered) {
			/*
			 * Inject edge-triggered interrupts to avoid losing
			 * events while masked. Linux can handle rare spurious
			 * interrupts.
			 */
			entry = phys_ioapic->shadow_redir_table[pin];
			irq_msg = ioapic_translate_redir_entry(cell, pin,
							       entry);
			if (irq_msg.valid)
				apic_send_irq(irq_msg);
		}
	}
}

int ioapic_init(void)
{
	int err;

	err = ioapic_cell_init(&root_cell);
	if (err)
		return err;

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

	if (num_phys_ioapics == 0)
		return;
	if (irqchip) {
		pin_bitmap = root_cell.ioapic_pin_bitmap;
		ioapic_mask_pins(&root_cell, pin_bitmap, PINS_ACTIVE);
	}
	ioapic_mask_pins(&root_cell, ~pin_bitmap, PINS_MASKED);
}

static struct phys_ioapic *
ioapic_get_or_add_phys(const struct jailhouse_irqchip *irqchip)
{
	struct phys_ioapic *phys_ioapic;
	unsigned int n, index;
	int err;

	for_each_phys_ioapic(phys_ioapic, n)
		if (phys_ioapic->base_addr == irqchip->address)
			return phys_ioapic;

	if (num_phys_ioapics == IOAPIC_MAX_CHIPS)
		return NULL;

	phys_ioapic->reg_base = page_alloc(&remap_pool, 1);
	if (!phys_ioapic->reg_base)
		return NULL;
	err = paging_create(&hv_paging_structs, irqchip->address, PAGE_SIZE,
			    (unsigned long)phys_ioapic->reg_base,
			    PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE,
			    PAGING_NON_COHERENT);
	if (err) {
		page_free(&remap_pool, phys_ioapic->reg_base, 1);
		return NULL;
	}

	phys_ioapic->base_addr = irqchip->address;
	num_phys_ioapics++;

	for (index = 0; index < IOAPIC_NUM_PINS * 2; index++)
		phys_ioapic->shadow_redir_table[index / 2].raw[index % 2] =
			ioapic_reg_read(phys_ioapic,
					IOAPIC_REDIR_TBL_START + index);

	return phys_ioapic;
}

int ioapic_cell_init(struct cell *cell)
{
	const struct jailhouse_irqchip *irqchip =
		jailhouse_cell_irqchips(cell->config);
	struct phys_ioapic *phys_ioapic;
	unsigned int n;

	if (cell->config->num_irqchips > IOAPIC_MAX_CHIPS)
		return -ERANGE;

	for (n = 0; n < cell->config->num_irqchips; n++, irqchip++) {
		phys_ioapic = ioapic_get_or_add_phys(irqchip);
		if (!phys_ioapic)
			return -ENOMEM;

		/* this still assumes IOAPIC_MAX_CHIPS == 1 */
		cell->ioapic_id = (u16)irqchip->id;
		cell->ioapic_iommu = (u8)(irqchip->id >> 16);
		cell->ioapic_pin_bitmap = irqchip->pin_bitmap;

		if (cell != &root_cell) {
			root_cell.ioapic_pin_bitmap &= ~irqchip->pin_bitmap;
			ioapic_mask_pins(cell, irqchip->pin_bitmap,
					 PINS_MASKED);
		}
	}

	return 0;
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

		entry = phys_ioapics[0].shadow_redir_table[pin];
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
	union ioapic_redir_entry *shadow_table;
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
			*value = ioapic_reg_read(&phys_ioapics[0], index);
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
			shadow_table = phys_ioapics[0].shadow_redir_table;
			*value = shadow_table[index / 2].raw[index % 2];
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
		mmio_write32(phys_ioapics[0].reg_base + IOAPIC_REG_EOI,
			     *value);
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
	union ioapic_redir_entry *shadow_table;
	int index;

	if (num_phys_ioapics == 0)
		return;
	shadow_table = phys_ioapics[0].shadow_redir_table;
	/* write in reverse order to preserve the mask as long as needed */
	for (index = IOAPIC_NUM_PINS * 2 - 1; index >= 0; index--)
		ioapic_reg_write(&phys_ioapics[0],
				 IOAPIC_REDIR_TBL_START + index,
				 shadow_table[index / 2].raw[index % 2]);
}
