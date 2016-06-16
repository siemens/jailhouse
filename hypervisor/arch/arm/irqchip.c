/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/entry.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <asm/gic_common.h>
#include <asm/irqchip.h>
#include <asm/platform.h>
#include <asm/setup.h>
#include <asm/sysregs.h>

/* AMBA's biosfood */
#define AMBA_DEVICE	0xb105f00d

void *gicd_base;
unsigned long gicd_size;

/*
 * The init function must be called after the MMU setup, and whilst in the
 * per-cpu setup, which means that a bool must be set by the master CPU
 */
static bool irqchip_is_init;
static struct irqchip_ops irqchip;

bool spi_in_cell(struct cell *cell, unsigned int spi)
{
	/* FIXME: Change the configuration to a bitmask range */
	u32 spi_mask;

	if (spi >= 64)
		return false;
	else if (spi >= 32)
		spi_mask = cell->arch.spis >> 32;
	else
		spi_mask = cell->arch.spis;

	return spi_mask & (1 << (spi & 31));
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

int irqchip_cpu_reset(struct per_cpu *cpu_data)
{
	cpu_data->pending_irqs_head = cpu_data->pending_irqs_tail = 0;

	return irqchip.cpu_reset(cpu_data, false);
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

static const struct jailhouse_irqchip *
irqchip_find_config(struct jailhouse_cell_desc *config)
{
	const struct jailhouse_irqchip *irq_config =
		jailhouse_cell_irqchips(config);

	if (config->num_irqchips)
		return irq_config;
	else
		return NULL;
}

int irqchip_cell_init(struct cell *cell)
{
	const struct jailhouse_irqchip *pins = irqchip_find_config(cell->config);

	cell->arch.spis = (pins ? pins->pin_bitmap : 0);

	return irqchip.cell_init(cell);
}

void irqchip_cell_exit(struct cell *cell)
{
	const struct jailhouse_irqchip *root_pins =
		irqchip_find_config(root_cell.config);

	/* might be called by arch_shutdown while rolling back
	 * a failed setup */
	if (!irqchip_is_init)
		return;

	if (root_pins)
		root_cell.arch.spis |= cell->arch.spis & root_pins->pin_bitmap;

	irqchip.cell_exit(cell);
}

void irqchip_root_cell_shrink(struct cell *cell)
{
	root_cell.arch.spis &= ~(cell->arch.spis);
}

/* Only the GIC is implemented */
extern struct irqchip_ops gic_irqchip;

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
		memcpy(&irqchip, &gic_irqchip, sizeof(struct irqchip_ops));
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
