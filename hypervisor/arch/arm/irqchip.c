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

static int irqchip_init_pending(struct per_cpu *cpu_data)
{
	struct pending_irq *pend_array;

	if (cpu_data->pending_irqs == NULL) {
		cpu_data->pending_irqs = pend_array = page_alloc(&mem_pool, 1);
		if (pend_array == NULL)
			return -ENOMEM;
	} else {
		pend_array = cpu_data->pending_irqs;
	}

	memset(pend_array, 0, PAGE_SIZE);

	cpu_data->pending_irqs = pend_array;
	cpu_data->first_pending = NULL;

	return 0;
}

/*
 * Find the first available pending struct for insertion. The `prev' pointer is
 * set to the previous pending interrupt, if any, to help inserting the new one
 * into the list.
 * Returns NULL when no slot is available
 */
static struct pending_irq* get_pending_slot(struct per_cpu *cpu_data,
					    struct pending_irq **prev)
{
	u32 i, pending_idx;
	struct pending_irq *pending = cpu_data->first_pending;

	*prev = NULL;

	for (i = 0; i < MAX_PENDING_IRQS; i++) {
		pending_idx = pending - cpu_data->pending_irqs;
		if (pending == NULL || i < pending_idx)
			return cpu_data->pending_irqs + i;

		*prev = pending;
		pending = pending->next;
	}

	return NULL;
}

int irqchip_insert_pending(struct per_cpu *cpu_data, struct pending_irq *irq)
{
	struct pending_irq *prev = NULL;
	struct pending_irq *slot;

	spin_lock(&cpu_data->gic_lock);

	slot = get_pending_slot(cpu_data, &prev);
	if (slot == NULL) {
		spin_unlock(&cpu_data->gic_lock);
		return -ENOMEM;
	}

	/*
	 * Don't override the pointers yet, they may be read by the injection
	 * loop. Odds are astronomically low, but hey.
	 */
	memcpy(slot, irq, sizeof(struct pending_irq) - 2 * sizeof(void *));
	slot->prev = prev;
	if (prev) {
		slot->next = prev->next;
		prev->next = slot;
	} else {
		slot->next = cpu_data->first_pending;
		cpu_data->first_pending = slot;
	}
	if (slot->next)
		slot->next->prev = slot;

	spin_unlock(&cpu_data->gic_lock);

	return 0;
}

int irqchip_set_pending(struct per_cpu *cpu_data, u32 irq_id, bool try_inject)
{
	struct pending_irq pending;

	pending.virt_id = irq_id;
	/* Priority must be less than ICC_PMR */
	pending.priority = 0;

	if (is_sgi(irq_id)) {
		pending.hw = 0;
		pending.type.sgi.maintenance = 0;
		pending.type.sgi.cpuid = 0;
	} else {
		pending.hw = 1;
		pending.type.irq = irq_id;
	}

	if (try_inject && irqchip.inject_irq(cpu_data, &pending) == 0)
		return 0;

	return irqchip_insert_pending(cpu_data, &pending);
}

/*
 * Only executed by `irqchip_inject_pending' on a CPU to inject its own stuff.
 */
int irqchip_remove_pending(struct per_cpu *cpu_data, struct pending_irq *irq)
{
	spin_lock(&cpu_data->gic_lock);

	if (cpu_data->first_pending == irq)
		cpu_data->first_pending = irq->next;
	if (irq->prev)
		irq->prev->next = irq->next;
	if (irq->next)
		irq->next->prev = irq->prev;

	spin_unlock(&cpu_data->gic_lock);

	return 0;
}

int irqchip_inject_pending(struct per_cpu *cpu_data)
{
	int err;
	struct pending_irq *pending = cpu_data->first_pending;

	while (pending != NULL) {
		err = irqchip.inject_irq(cpu_data, pending);
		if (err == -EBUSY)
			/* The list registers are full. */
			break;
		else
			/*
			 * Removal only changes the pointers, but does not
			 * deallocate anything.
			 * Concurrent accesses are avoided with the spinlock,
			 * but the `next' pointer of the current pending object
			 * may be rewritten by an external insert before or
			 * after this removal, which isn't an issue.
			 */
			irqchip_remove_pending(cpu_data, pending);

		pending = pending->next;
	}

	return 0;
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
	int err;

	err = irqchip_init_pending(cpu_data);
	if (err)
		return err;

	if (irqchip.cpu_init)
		return irqchip.cpu_init(cpu_data);

	return 0;
}

int irqchip_cpu_reset(struct per_cpu *cpu_data)
{
	int err;

	err = irqchip_init_pending(cpu_data);
	if (err)
		return err;

	if (irqchip.cpu_reset)
		return irqchip.cpu_reset(cpu_data, false);

	return 0;
}

void irqchip_cpu_shutdown(struct per_cpu *cpu_data)
{
	/*
	 * The GIC backend must take care of only resetting the hyp interface if
	 * it has been initialised: this function may be executed during the
	 * setup phase.
	 */
	if (irqchip.cpu_reset)
		irqchip.cpu_reset(cpu_data, true);
}

int irqchip_mmio_access(struct mmio_access *access)
{
	if (irqchip.mmio_access)
		return irqchip.mmio_access(access);

	return TRAP_UNHANDLED;
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
	}

	if (irqchip.init) {
		err = irqchip.init();
		irqchip_is_init = true;

		return err;
	}

err_no_distributor:
	printk("GIC: no distributor found\n");
	arch_unmap_device(gicd_base, gicd_size);

	return -ENODEV;
}
