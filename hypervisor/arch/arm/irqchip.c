/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/entry.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <asm/control.h>
#include <asm/gic_common.h>
#include <asm/irqchip.h>
#include <asm/platform.h>
#include <asm/setup.h>
#include <asm/sysregs.h>

/* AMBA's biosfood */
#define AMBA_DEVICE	0xb105f00d

#define for_each_irqchip(chip, config, counter)				\
	for ((chip) = jailhouse_cell_irqchips(config), (counter) = 0;	\
	     (counter) < (config)->num_irqchips;			\
	     (chip)++, (counter)++)

extern struct irqchip_ops irqchip;

void *gicd_base;
unsigned long gicd_size;

/*
 * The init function must be called after the MMU setup, and whilst in the
 * per-cpu setup, which means that a bool must be set by the master CPU
 */
static bool irqchip_is_init;

bool irqchip_irq_in_cell(struct cell *cell, unsigned int irq_id)
{
	if (irq_id >= sizeof(cell->arch.irq_bitmap) * 8)
		return false;

	return (cell->arch.irq_bitmap[irq_id / 32] & (1 << (irq_id % 32))) != 0;
}

void irqchip_set_pending(struct per_cpu *cpu_data, u16 irq_id)
{
	bool local_injection = (this_cpu_data() == cpu_data);
	unsigned int new_tail;

	if (local_injection && irqchip.inject_irq(cpu_data, irq_id) != -EBUSY)
		return;

	spin_lock(&cpu_data->pending_irqs_lock);

	new_tail = (cpu_data->pending_irqs_tail + 1) % MAX_PENDING_IRQS;

	/* Queue space available? */
	if (new_tail != cpu_data->pending_irqs_head) {
		cpu_data->pending_irqs[cpu_data->pending_irqs_tail] = irq_id;
		cpu_data->pending_irqs_tail = new_tail;
		/*
		 * Make the change to pending_irqs_tail visible before the
		 * caller sends SGI_INJECT.
		 */
		memory_barrier();
	}

	spin_unlock(&cpu_data->pending_irqs_lock);

	/*
	 * The list registers are full, trigger maintenance interrupt if we are
	 * on the target CPU. In the other case, the caller will send a
	 * SGI_INJECT, and irqchip_inject_pending will take care.
	 */
	if (local_injection)
		irqchip.enable_maint_irq(true);
}

void irqchip_inject_pending(struct per_cpu *cpu_data)
{
	u16 irq_id;

	while (cpu_data->pending_irqs_head != cpu_data->pending_irqs_tail) {
		irq_id = cpu_data->pending_irqs[cpu_data->pending_irqs_head];

		if (irqchip.inject_irq(cpu_data, irq_id) == -EBUSY) {
			/*
			 * The list registers are full, trigger maintenance
			 * interrupt and leave.
			 */
			irqchip.enable_maint_irq(true);
			return;
		}

		cpu_data->pending_irqs_head =
			(cpu_data->pending_irqs_head + 1) % MAX_PENDING_IRQS;
	}

	/*
	 * The software interrupt queue is empty - turn off the maintenance
	 * interrupt.
	 */
	irqchip.enable_maint_irq(false);
}

void irqchip_handle_irq(struct per_cpu *cpu_data)
{
	irqchip.handle_irq(cpu_data);
}

void irqchip_eoi_irq(u32 irqn, bool deactivate)
{
	irqchip.eoi_irq(irqn, deactivate);
}

int irqchip_send_sgi(struct sgi *sgi)
{
	return irqchip.send_sgi(sgi);
}

int irqchip_cpu_init(struct per_cpu *cpu_data)
{
	return irqchip.cpu_init(cpu_data);
}

void irqchip_cpu_reset(struct per_cpu *cpu_data)
{
	cpu_data->pending_irqs_head = cpu_data->pending_irqs_tail = 0;

	irqchip.cpu_reset(cpu_data, false);
}

void irqchip_cpu_shutdown(struct per_cpu *cpu_data)
{
	/*
	 * The GIC backend must take care of only resetting the hyp interface if
	 * it has been initialised: this function may be executed during the
	 * setup phase.
	 */
	irqchip.cpu_reset(cpu_data, true);
}

int irqchip_cell_init(struct cell *cell)
{
	const struct jailhouse_irqchip *chip;
	unsigned int n, pos;
	int err;

	for_each_irqchip(chip, cell->config, n) {
		if (chip->address != (unsigned long)gicd_base)
			continue;
		if (chip->pin_base % 32 != 0 ||
		    chip->pin_base + sizeof(chip->pin_bitmap) * 8 >
		    sizeof(cell->arch.irq_bitmap) * 8)
			return trace_error(-EINVAL);
		memcpy(&cell->arch.irq_bitmap[chip->pin_base / 32],
		       chip->pin_bitmap, sizeof(chip->pin_bitmap));
	}
	/*
	 * Permit direct access to all SGIs and PPIs except for those used by
	 * the hypervisor.
	 */
	cell->arch.irq_bitmap[0] = ~((1 << SGI_INJECT) | (1 << SGI_CPU_OFF) |
				     (1 << MAINTENANCE_IRQ));

	err = irqchip.cell_init(cell);
	if (err)
		return err;

	if (cell != &root_cell) {
		for_each_irqchip(chip, cell->config, n) {
			if (chip->address != (unsigned long)gicd_base)
				continue;
			for (pos = 0; pos < ARRAY_SIZE(chip->pin_bitmap); pos++)
				root_cell.arch.irq_bitmap[chip->pin_base/32] &=
					~chip->pin_bitmap[pos];
		}

		for (n = 32; n < sizeof(cell->arch.irq_bitmap) * 8; n++) {
			if (irqchip_irq_in_cell(cell, n))
				irqchip.adjust_irq_target(cell, n);
			if (irqchip_irq_in_cell(&root_cell, n))
				irqchip.adjust_irq_target(&root_cell, n);
		}
	}

	return 0;
}

void irqchip_cell_exit(struct cell *cell)
{
	const struct jailhouse_irqchip *chip;
	unsigned int n, pos;

	/* might be called by arch_shutdown while rolling back
	 * a failed setup */
	if (!irqchip_is_init)
		return;

	/* set all pins of the old cell in the root cell */
	for_each_irqchip(chip, cell->config, n) {
		if (chip->address != (unsigned long)gicd_base)
			continue;
		for (pos = 0; pos < ARRAY_SIZE(chip->pin_bitmap); pos++)
			root_cell.arch.irq_bitmap[chip->pin_base / 32] |=
				chip->pin_bitmap[pos];
	}

	/* mask out pins again that actually didn't belong to the root cell */
	for_each_irqchip(chip, root_cell.config, n) {
		if (chip->address != (unsigned long)gicd_base)
			continue;
		for (pos = 0; pos < ARRAY_SIZE(chip->pin_bitmap); pos++)
			root_cell.arch.irq_bitmap[chip->pin_base / 32] &=
				chip->pin_bitmap[pos];
	}

	if (irqchip.cell_exit)
		irqchip.cell_exit(cell);
}

int irqchip_init(void)
{
	int i, err;
	u32 pidr2, cidr;
	u32 dev_id = 0;

	/* Only executed on master CPU */
	if (irqchip_is_init)
		return 0;

	/* FIXME: parse device tree */
	gicd_base = GICD_BASE;
	gicd_size = GICD_SIZE;

	if ((err = arch_map_device(gicd_base, gicd_base, gicd_size)) != 0)
		return err;

	for (i = 3; i >= 0; i--) {
		cidr = mmio_read32(gicd_base + GICD_CIDR0 + i * 4);
		dev_id |= cidr << i * 8;
	}
	if (dev_id != AMBA_DEVICE)
		goto err_no_distributor;

	/* Probe the GIC version */
	pidr2 = mmio_read32(gicd_base + GICD_PIDR2);
	switch (GICD_PIDR2_ARCH(pidr2)) {
	case 0x2:
	case 0x3:
	case 0x4:
		break;
	default:
		goto err_no_distributor;
	}

	if (irqchip.init) {
		err = irqchip.init();
		irqchip_is_init = true;

		return err;
	}

err_no_distributor:
	printk("GIC: no supported distributor found\n");
	arch_unmap_device(gicd_base, gicd_size);

	return -ENODEV;
}
