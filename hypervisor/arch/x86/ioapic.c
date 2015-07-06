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

#define IOAPIC_MAX_CHIPS	(PAGE_SIZE / sizeof(struct cell_ioapic))

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

#define for_each_cell_ioapic(ioapic, cell, counter)		\
	for ((ioapic) = (cell)->arch.ioapics, (counter) = 0;	\
	     (counter) < (cell)->arch.num_ioapics;		\
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
ioapic_translate_redir_entry(struct cell_ioapic *ioapic, unsigned int pin,
			     union ioapic_redir_entry entry)
{
	struct apic_irq_message irq_msg = { .valid = 0 };
	unsigned int idx, ioapic_id;

	if (iommu_cell_emulates_ir(ioapic->cell)) {
		if (!entry.remap.remapped)
			return irq_msg;

		idx = entry.remap.int_index | (entry.remap.int_index15 << 15);
		ioapic_id = ioapic->info->id;

		return iommu_get_remapped_root_int(ioapic_id >> 16,
						   (u16)ioapic_id, pin, idx);
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

static int ioapic_virt_redir_write(struct cell_ioapic *ioapic,
				   unsigned int reg, u32 value)
{
	unsigned int pin = (reg - IOAPIC_REDIR_TBL_START) / 2;
	struct phys_ioapic *phys_ioapic = ioapic->phys_ioapic;
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

	irq_msg = ioapic_translate_redir_entry(ioapic, pin, entry);

	result = iommu_map_interrupt(ioapic->cell, (u16)ioapic->info->id, pin,
				     irq_msg);
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

static void ioapic_mask_cell_pins(struct cell_ioapic *ioapic,
				  enum ioapic_handover handover)
{
	struct phys_ioapic *phys_ioapic = ioapic->phys_ioapic;
	struct apic_irq_message irq_msg;
	union ioapic_redir_entry entry;
	unsigned int pin, reg;

	for (pin = 0; pin < IOAPIC_NUM_PINS; pin++) {
		if (!(ioapic->pin_bitmap & (1UL << pin)))
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
			irq_msg = ioapic_translate_redir_entry(ioapic, pin,
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

void ioapic_prepare_handover(void)
{
	enum ioapic_handover handover;
	struct cell_ioapic *ioapic;
	struct cell *cell;
	unsigned int n;

	for_each_cell(cell) {
		handover = (cell == &root_cell) ? PINS_ACTIVE : PINS_MASKED;
		for_each_cell_ioapic(ioapic, cell, n)
			ioapic_mask_cell_pins(ioapic, handover);
	}
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
		return trace_error(NULL);

	phys_ioapic->reg_base = page_alloc(&remap_pool, 1);
	if (!phys_ioapic->reg_base)
		return trace_error(NULL);
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

static struct cell_ioapic *ioapic_find_by_address(struct cell *cell,
						  unsigned long address)
{
	struct cell_ioapic *ioapic;
	unsigned int n;

	for_each_cell_ioapic(ioapic, cell, n) {
		unsigned long base = ioapic->info->address;

		if (address >= base && address < base + PAGE_SIZE)
			return ioapic;
	}
	return NULL;
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
	struct cell_ioapic *ioapic;
	u32 index, entry;

	ioapic = ioapic_find_by_address(cell, addr);
	if (!ioapic)
		return 0;

	switch (addr - ioapic->info->address) {
	case IOAPIC_REG_INDEX:
		if (is_write)
			ioapic->index_reg_val = *value;
		else
			*value = ioapic->index_reg_val;
		return 1;
	case IOAPIC_REG_DATA:
		index = ioapic->index_reg_val;

		if (index == IOAPIC_ID || index == IOAPIC_VER) {
			if (is_write)
				goto invalid_access;
			*value = ioapic_reg_read(ioapic->phys_ioapic, index);
			return 1;
		}

		if (index < IOAPIC_REDIR_TBL_START ||
		    index > IOAPIC_REDIR_TBL_END)
			goto invalid_access;

		entry = (index - IOAPIC_REDIR_TBL_START) / 2;
		if ((ioapic->pin_bitmap & (1UL << entry)) == 0)
			goto invalid_access;

		if (is_write) {
			if (ioapic_virt_redir_write(ioapic, index, *value) < 0)
				goto invalid_access;
		} else {
			index -= IOAPIC_REDIR_TBL_START;
			shadow_table = ioapic->phys_ioapic->shadow_redir_table;
			*value = shadow_table[index / 2].raw[index % 2];
		}
		return 1;
	case IOAPIC_REG_EOI:
		if (!is_write || ioapic->pin_bitmap == 0)
			goto invalid_access;
		/*
		 * Just write the EOI if the cell has any assigned pin. It
		 * would be complex to virtualize it in a way that cells are
		 * unable to ack vectors of other cells. It is therefore not
		 * recommended to use level-triggered IOAPIC interrupts in
		 * non-root cells.
		 */
		mmio_write32(ioapic->phys_ioapic->reg_base + IOAPIC_REG_EOI,
			     *value);
		return 1;
	}

invalid_access:
	panic_printk("FATAL: Invalid IOAPIC %s, reg: %x, index: %x\n",
		     is_write ? "write" : "read", addr - ioapic->info->address,
		     ioapic->index_reg_val);
	return -1;
}

int ioapic_cell_init(struct cell *cell)
{
	const struct jailhouse_irqchip *irqchip =
		jailhouse_cell_irqchips(cell->config);
	struct cell_ioapic *ioapic, *root_ioapic;
	struct phys_ioapic *phys_ioapic;
	unsigned int n;

	if (cell->config->num_irqchips == 0)
		return 0;
	if (cell->config->num_irqchips > IOAPIC_MAX_CHIPS)
		return trace_error(-ERANGE);

	cell->arch.ioapics = page_alloc(&mem_pool, 1);
	if (!cell->arch.ioapics)
		return -ENOMEM;

	for (n = 0; n < cell->config->num_irqchips; n++, irqchip++) {
		phys_ioapic = ioapic_get_or_add_phys(irqchip);
		if (!phys_ioapic)
			return -ENOMEM;

		ioapic = &cell->arch.ioapics[n];
		ioapic->info = irqchip;
		ioapic->cell = cell;
		ioapic->phys_ioapic = phys_ioapic;
		ioapic->pin_bitmap = (u32)irqchip->pin_bitmap;
		cell->arch.num_ioapics++;

		if (cell != &root_cell) {
			root_ioapic = ioapic_find_by_address(&root_cell,
							     irqchip->address);
			if (root_ioapic) {
				root_ioapic->pin_bitmap &= ~ioapic->pin_bitmap;
				ioapic_mask_cell_pins(ioapic, PINS_MASKED);
			}
		}
	}

	return 0;
}

void ioapic_cell_exit(struct cell *cell)
{
	struct cell_ioapic *ioapic, *root_ioapic;
	unsigned int n;

	for_each_cell_ioapic(ioapic, cell, n) {
		ioapic_mask_cell_pins(ioapic, PINS_MASKED);

		root_ioapic = ioapic_find_by_address(&root_cell,
						     ioapic->info->address);
		if (root_ioapic)
			root_ioapic->pin_bitmap |=
				ioapic->pin_bitmap &
				root_ioapic->info->pin_bitmap;
	}

	page_free(&mem_pool, cell->arch.ioapics, 1);
}

void ioapic_config_commit(struct cell *cell_added_removed)
{
	union ioapic_redir_entry entry;
	struct cell_ioapic *ioapic;
	unsigned int pin, reg, n;

	if (!cell_added_removed)
		return;

	for_each_cell_ioapic(ioapic, &root_cell, n)
		for (pin = 0; pin < IOAPIC_NUM_PINS; pin++) {
			if (!(ioapic->pin_bitmap & (1UL << pin)))
				continue;

			entry = ioapic->phys_ioapic->shadow_redir_table[pin];
			reg = IOAPIC_REDIR_TBL_START + pin * 2;

			/* write high word first to preserve mask initially */
			if (ioapic_virt_redir_write(ioapic, reg + 1,
						    entry.raw[1]) < 0 ||
			    ioapic_virt_redir_write(ioapic, reg,
						    entry.raw[0]) < 0) {
				panic_printk("FATAL: Unsupported IOAPIC "
					     "state, pin %d\n", pin);
				panic_stop();
			}
		}
}

void ioapic_shutdown(void)
{
	union ioapic_redir_entry *shadow_table;
	struct phys_ioapic *phys_ioapic;
	unsigned int n;
	int index;

	for_each_phys_ioapic(phys_ioapic, n) {
		shadow_table = phys_ioapic->shadow_redir_table;

		/* write in reverse order to preserve the mask as long as
		 * needed */
		for (index = IOAPIC_NUM_PINS * 2 - 1; index >= 0; index--)
			ioapic_reg_write(phys_ioapic,
				IOAPIC_REDIR_TBL_START + index,
				shadow_table[index / 2].raw[index % 2]);
	}
}
