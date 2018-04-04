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

#include <jailhouse/control.h>
#include <jailhouse/entry.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/unit.h>
#include <asm/control.h>
#include <asm/gic.h>
#include <asm/irqchip.h>
#include <asm/sysregs.h>

#define for_each_irqchip(chip, config, counter)				\
	for ((chip) = jailhouse_cell_irqchips(config), (counter) = 0;	\
	     (counter) < (config)->num_irqchips;			\
	     (chip)++, (counter)++)

DEFINE_SPINLOCK(dist_lock);

void *gicd_base;

/*
 * The init function must be called after the MMU setup, and whilst in the
 * per-cpu setup, which means that a bool must be set by the master CPU
 */
static bool irqchip_is_init;

static struct irqchip irqchip;

/*
 * Most of the GIC distributor writes only reconfigure the IRQs corresponding to
 * the bits of the written value, by using separate `set' and `clear' registers.
 * Such registers can be handled by setting the `is_poke' boolean, which allows
 * to simply restrict the mmio->value with the cell configuration mask.
 * Others, such as the priority registers, will need to be read and written back
 * with a restricted value, by using the distributor lock.
 */
static enum mmio_result
restrict_bitmask_access(struct mmio_access *mmio, unsigned int reg_index,
			unsigned int bits_per_irq, bool is_poke)
{
	struct cell *cell = this_cell();
	unsigned int irq;
	unsigned long access_mask = 0;
	/*
	 * In order to avoid division, the number of bits per irq is limited
	 * to powers of 2 for the moment.
	 */
	unsigned long irqs_per_reg = 32 >> ffsl(bits_per_irq);
	unsigned long irq_bits = (1 << bits_per_irq) - 1;
	/* First, extract the first interrupt affected by this access */
	unsigned int first_irq = reg_index * irqs_per_reg;

	for (irq = 0; irq < irqs_per_reg; irq++)
		if (irqchip_irq_in_cell(cell, first_irq + irq))
			access_mask |= irq_bits << (irq * bits_per_irq);

	if (!mmio->is_write) {
		/* Restrict the read value */
		mmio_perform_access(gicd_base, mmio);
		mmio->value &= access_mask;
		return MMIO_HANDLED;
	}

	if (!is_poke) {
		/*
		 * Modify the existing value of this register by first reading
		 * it into mmio->value
		 * Relies on a spinlock since we need two mmio accesses.
		 */
		unsigned long access_val = mmio->value;

		spin_lock(&dist_lock);

		mmio->is_write = false;
		mmio_perform_access(gicd_base, mmio);
		mmio->is_write = true;

		mmio->value &= ~access_mask;
		mmio->value |= access_val & access_mask;
		mmio_perform_access(gicd_base, mmio);

		spin_unlock(&dist_lock);
	} else {
		mmio->value &= access_mask;
		mmio_perform_access(gicd_base, mmio);
	}
	return MMIO_HANDLED;
}

void gic_handle_sgir_write(struct sgi *sgi)
{
	struct per_cpu *cpu_data = this_cpu_data();
	unsigned int cpu, target;
	u64 cluster;

	if (sgi->routing_mode == 2)
		/* Route to the caller itself */
		irqchip_set_pending(cpu_data, sgi->id);
	else
		for_each_cpu(cpu, cpu_data->cell->cpu_set) {
			if (sgi->routing_mode == 1) {
				/* Route to all (cell) CPUs but the caller. */
				if (cpu == cpu_data->cpu_id)
					continue;
			} else {
				target = irqchip_get_cpu_target(cpu);
				cluster = irqchip_get_cluster_target(cpu);

				/* Route to target CPUs in cell */
				if ((sgi->cluster_id != cluster) ||
				    !(sgi->targets & target))
					continue;
			}

			irqchip_set_pending(per_cpu(cpu), sgi->id);
		}
}

static enum mmio_result gic_handle_dist_access(void *arg,
					       struct mmio_access *mmio)
{
	unsigned long reg = mmio->address;
	enum mmio_result ret;

	switch (reg) {
	case REG_RANGE(GICD_IROUTER, 1024, 8):
		ret = irqchip.handle_irq_route(mmio, (reg - GICD_IROUTER) / 8);
		break;

	case REG_RANGE(GICD_ITARGETSR, 1024, 1):
		ret = irqchip.handle_irq_target(mmio, reg - GICD_ITARGETSR);
		break;

	case REG_RANGE(GICD_ICENABLER, 32, 4):
	case REG_RANGE(GICD_ISENABLER, 32, 4):
	case REG_RANGE(GICD_ICPENDR, 32, 4):
	case REG_RANGE(GICD_ISPENDR, 32, 4):
	case REG_RANGE(GICD_ICACTIVER, 32, 4):
	case REG_RANGE(GICD_ISACTIVER, 32, 4):
		ret = restrict_bitmask_access(mmio, (reg & 0x7f) / 4, 1, true);
		break;

	case REG_RANGE(GICD_IGROUPR, 32, 4):
		ret = restrict_bitmask_access(mmio, (reg & 0x7f) / 4, 1, false);
		break;

	case REG_RANGE(GICD_ICFGR, 64, 4):
		ret = restrict_bitmask_access(mmio, (reg & 0xff) / 4, 2, false);
		break;

	case REG_RANGE(GICD_IPRIORITYR, 255, 4):
		ret = restrict_bitmask_access(mmio, (reg & 0x3ff) / 4, 8,
					      false);
		break;

	default:
		ret = irqchip.handle_dist_access(mmio);
	}

	return ret;
}

void irqchip_handle_irq(void)
{
	unsigned int count_event = 1;
	bool handled = false;
	u32 irq_id;

	while (1) {
		/* Read IAR1: set 'active' state */
		irq_id = irqchip.read_iar_irqn();

		if (irq_id == 0x3ff) /* Spurious IRQ */
			break;

		/* Handle IRQ */
		if (is_sgi(irq_id)) {
			arch_handle_sgi(irq_id, count_event);
			handled = true;
		} else {
			handled = arch_handle_phys_irq(irq_id, count_event);
		}
		count_event = 0;

		/*
		 * Write EOIR1: drop priority, but stay active if handled is
		 * false.
		 * This allows to not be re-interrupted by a level-triggered
		 * interrupt that needs handling in the guest (e.g. timer)
		 */
		irqchip.eoi_irq(irq_id, handled);
	}
}

bool irqchip_irq_in_cell(struct cell *cell, unsigned int irq_id)
{
	if (irq_id >= sizeof(cell->arch.irq_bitmap) * 8)
		return false;

	return (cell->arch.irq_bitmap[irq_id / 32] & (1 << (irq_id % 32))) != 0;
}

bool irqchip_has_pending_irqs(void)
{
	return irqchip.has_pending_irqs();
}

void irqchip_set_pending(struct per_cpu *cpu_data, u16 irq_id)
{
	struct pending_irqs *pending = &cpu_data->pending_irqs;
	bool local_injection = (this_cpu_data() == cpu_data);
	const u16 sender = this_cpu_data()->cpu_id;
	unsigned int new_tail;
	struct sgi sgi;

	if (!cpu_data) {
		/* Injection via GICD */
		mmio_write32(gicd_base + GICD_ISPENDR + (irq_id / 32) * 4,
			     1 << (irq_id % 32));
		return;
	}

	if (local_injection &&
	    irqchip.inject_irq(cpu_data, irq_id, sender) != -EBUSY)
		return;

	spin_lock(&pending->lock);

	new_tail = (pending->tail + 1) % MAX_PENDING_IRQS;

	/* Queue space available? */
	if (new_tail != pending->head) {
		pending->irqs[pending->tail] = irq_id;
		pending->sender[pending->tail] = sender;
		pending->tail = new_tail;
		/*
		 * Make the change to pending_irqs.tail visible before the
		 * caller sends SGI_INJECT.
		 */
		memory_barrier();
	}

	spin_unlock(&pending->lock);

	/*
	 * The list registers are full, trigger maintenance interrupt if we are
	 * on the target CPU. In the other case, send SGI_INJECT to the target
	 * CPU.
	 */
	if (local_injection) {
		irqchip.enable_maint_irq(true);
	} else {
		sgi.targets = irqchip_get_cpu_target(cpu_data->cpu_id);
		sgi.cluster_id = irqchip_get_cluster_target(cpu_data->cpu_id);
		sgi.routing_mode = 0;
		sgi.id = SGI_INJECT;

		irqchip_send_sgi(&sgi);
	}
}

void irqchip_inject_pending(struct per_cpu *cpu_data)
{
	struct pending_irqs *pending = &cpu_data->pending_irqs;
	u16 irq_id, sender;

	while (pending->head != pending->tail) {
		irq_id = pending->irqs[pending->head];
		sender = pending->sender[pending->head];

		if (irqchip.inject_irq(cpu_data, irq_id, sender) == -EBUSY) {
			/*
			 * The list registers are full, trigger maintenance
			 * interrupt and leave.
			 */
			irqchip.enable_maint_irq(true);
			return;
		}

		pending->head = (pending->head + 1) % MAX_PENDING_IRQS;
	}

	/*
	 * The software interrupt queue is empty - turn off the maintenance
	 * interrupt.
	 */
	irqchip.enable_maint_irq(false);
}

int irqchip_send_sgi(struct sgi *sgi)
{
	return irqchip.send_sgi(sgi);
}

int irqchip_cpu_init(struct per_cpu *cpu_data)
{
	int err;

	/* Only execute once, on master CPU */
	if (!irqchip_is_init) {
		switch (system_config->platform_info.arm.gic_version) {
		case 2:
			irqchip = gicv2_irqchip;
			break;
		case 3:
			irqchip = gicv3_irqchip;
			break;
		default:
			return trace_error(-EINVAL);
		}

		gicd_base = paging_map_device(
				system_config->platform_info.arm.gicd_base,
				irqchip.gicd_size);
		if (!gicd_base)
			return -ENOMEM;

		err = irqchip.init();
		if (err)
			return err;

		irqchip_is_init = true;
	}

	return irqchip.cpu_init(cpu_data);
}

int irqchip_get_cpu_target(unsigned int cpu_id)
{
	return irqchip.get_cpu_target(cpu_id);
}

u64 irqchip_get_cluster_target(unsigned int cpu_id)
{
	return irqchip.get_cluster_target(cpu_id);
}

void irqchip_cpu_reset(struct per_cpu *cpu_data)
{
	cpu_data->pending_irqs.head = cpu_data->pending_irqs.tail = 0;

	irqchip.cpu_reset(cpu_data);
}

void irqchip_cpu_shutdown(struct per_cpu *cpu_data)
{
	struct pending_irqs *pending = &cpu_data->pending_irqs;
	int irq_id;

	/*
	 * The GIC implementation must take care of only resetting the hyp
	 * interface if it has been initialized because this function may be
	 * executed during the setup phase. It returns an error if the
	 * initialization do not take place yet.
	 */
	if (irqchip.cpu_shutdown(cpu_data) < 0)
		return;

	/*
	 * Migrate interrupts queued in the GICV.
	 * No locking required at this stage because no other CPU is able to
	 * inject anymore.
	 */
	do {
		irq_id = irqchip.get_pending_irq();
		if (irq_id >= 0)
			irqchip.inject_phys_irq(irq_id);
	} while (irq_id >= 0);

	/* Migrate interrupts queued in software. */
	while (pending->head != pending->tail) {
		irq_id = pending->irqs[pending->head];

		irqchip.inject_phys_irq(irq_id);

		pending->head = (pending->head + 1) % MAX_PENDING_IRQS;
	}
}

static int irqchip_cell_init(struct cell *cell)
{
	unsigned int mnt_irq = system_config->platform_info.arm.maintenance_irq;
	const struct jailhouse_irqchip *chip;
	unsigned int n, pos;
	int err;

	for_each_irqchip(chip, cell->config, n) {
		if (chip->address != system_config->platform_info.arm.gicd_base)
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
	cell->arch.irq_bitmap[0] = ~((1 << SGI_INJECT) | (1 << SGI_EVENT) |
				     (1 << mnt_irq));

	err = irqchip.cell_init(cell);
	if (err)
		return err;

	mmio_region_register(cell, system_config->platform_info.arm.gicd_base,
			     irqchip.gicd_size, gic_handle_dist_access, NULL);

	if (cell == &root_cell)
		return 0;

	for_each_irqchip(chip, cell->config, n) {
		if (chip->address != system_config->platform_info.arm.gicd_base)
			continue;
		for (pos = 0; pos < ARRAY_SIZE(chip->pin_bitmap); pos++)
			root_cell.arch.irq_bitmap[chip->pin_base / 32 + pos] &=
				~chip->pin_bitmap[pos];
	}

	return 0;
}

void irqchip_cell_reset(struct cell *cell)
{
	unsigned int n;
	u32 mask;

	/* mask and deactivate all SPIs belonging to the cell */
	for (n = 1; n < ARRAY_SIZE(cell->arch.irq_bitmap); n++) {
		mask = cell->arch.irq_bitmap[n];
		mmio_write32(gicd_base + GICD_ICENABLER + n * 4, mask);
		mmio_write32(gicd_base + GICD_ICACTIVER + n * 4, mask);
	}
}

static void irqchip_cell_exit(struct cell *cell)
{
	const struct jailhouse_irqchip *chip;
	unsigned int n, pos;

	/* might be called by arch_shutdown while rolling back
	 * a failed setup */
	if (!irqchip_is_init)
		return;

	/* ensure all SPIs of the cell are masked and deactivated */
	irqchip_cell_reset(cell);

	/* set all pins of the old cell in the root cell */
	for_each_irqchip(chip, cell->config, n) {
		if (chip->address != system_config->platform_info.arm.gicd_base)
			continue;
		for (pos = 0; pos < ARRAY_SIZE(chip->pin_bitmap); pos++)
			root_cell.arch.irq_bitmap[chip->pin_base / 32 + pos] |=
				chip->pin_bitmap[pos];
	}

	/* mask out pins again that actually didn't belong to the root cell */
	for_each_irqchip(chip, root_cell.config, n) {
		if (chip->address != system_config->platform_info.arm.gicd_base)
			continue;
		for (pos = 0; pos < ARRAY_SIZE(chip->pin_bitmap); pos++)
			root_cell.arch.irq_bitmap[chip->pin_base / 32 + pos] &=
				chip->pin_bitmap[pos];
	}

	if (irqchip.cell_exit)
		irqchip.cell_exit(cell);
}

void irqchip_config_commit(struct cell *cell_added_removed)
{
	unsigned int n;

	if (!cell_added_removed)
		return;

	for (n = 32; n < sizeof(cell_added_removed->arch.irq_bitmap) * 8; n++) {
		if (irqchip_irq_in_cell(cell_added_removed, n))
			irqchip.adjust_irq_target(cell_added_removed, n);
		if (irqchip_irq_in_cell(&root_cell, n))
			irqchip.adjust_irq_target(&root_cell, n);
	}
}

static unsigned int irqchip_mmio_count_regions(struct cell *cell)
{
	unsigned int regions = 1; /* GICD */

	if (system_config->platform_info.arm.gic_version >= 3)
		/* 1 GICR per CPU */
		regions += hypervisor_header.online_cpus;

	return regions;
}

static int irqchip_init(void)
{
	/* Setup the SPI bitmap */
	return irqchip_cell_init(&root_cell);
}

DEFINE_UNIT_SHUTDOWN_STUB(irqchip);
DEFINE_UNIT(irqchip, "irqchip");
