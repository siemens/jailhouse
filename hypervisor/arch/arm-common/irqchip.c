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
#include <asm/control.h>
#include <asm/gic.h>
#include <asm/irqchip.h>
#include <asm/sysregs.h>

/* AMBA's biosfood */
#define AMBA_DEVICE	0xb105f00d

#define for_each_irqchip(chip, config, counter)				\
	for ((chip) = jailhouse_cell_irqchips(config), (counter) = 0;	\
	     (counter) < (config)->num_irqchips;			\
	     (chip)++, (counter)++)

#define REG_RANGE(base, n, size)	\
		(base) ... ((base) + (n - 1) * (size))

/* The GICv2 interface numbering does not necessarily match the logical map */
u8 gicv2_target_cpu_map[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

DEFINE_SPINLOCK(dist_lock);

void *gicd_base;

/*
 * The init function must be called after the MMU setup, and whilst in the
 * per-cpu setup, which means that a bool must be set by the master CPU
 */
static bool irqchip_is_init;

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

static enum mmio_result handle_sgir_access(struct mmio_access *mmio)
{
	struct sgi sgi;
	unsigned long val = mmio->value;

	if (!mmio->is_write)
		return MMIO_HANDLED;

	sgi.targets = (val >> 16) & 0xff;
	sgi.routing_mode = (val >> 24) & 0x3;
	sgi.aff1 = 0;
	sgi.aff2 = 0;
	sgi.aff3 = 0;
	sgi.id = val & 0xf;

	gic_handle_sgir_write(&sgi, false);
	return MMIO_HANDLED;
}

void gic_handle_sgir_write(struct sgi *sgi, bool virt_input)
{
	struct per_cpu *cpu_data = this_cpu_data();
	unsigned long targets = sgi->targets;
	unsigned int cpu;

	if (sgi->routing_mode == 2) {
		/* Route to the caller itself */
		irqchip_set_pending(cpu_data, sgi->id);
		sgi->targets = (1 << cpu_data->cpu_id);
	} else {
		sgi->targets = 0;

		for_each_cpu(cpu, cpu_data->cell->cpu_set) {
			if (sgi->routing_mode == 1) {
				/* Route to all (cell) CPUs but the caller. */
				if (cpu == cpu_data->cpu_id)
					continue;
			} else if (virt_input) {
				if (!test_bit(arm_cpu_phys2virt(cpu),
					      &targets))
					continue;
			} else {
				/*
				 * When using a cpu map to target the different
				 * CPUs (GICv2), they are independent from the
				 * physical CPU IDs, so there is no need to
				 * translate them to the hypervisor's virtual
				 * IDs.
				 */
				if (!(targets & gicv2_target_cpu_map[cpu]))
					continue;
			}

			irqchip_set_pending(per_cpu(cpu), sgi->id);
			sgi->targets |= (1 << cpu);
		}
	}

	/* Let the other CPUS inject their SGIs */
	sgi->id = SGI_INJECT;
	irqchip_send_sgi(sgi);
}

static enum mmio_result gic_handle_dist_access(void *arg,
					       struct mmio_access *mmio)
{
	unsigned long reg = mmio->address;
	enum mmio_result ret;

	switch (reg) {
	case REG_RANGE(GICD_IROUTER, 1024, 8):
		ret = gic_handle_irq_route(mmio, (reg - GICD_IROUTER) / 8);
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

	case GICD_SGIR:
		ret = handle_sgir_access(mmio);
		break;

	case GICD_CTLR:
	case GICD_TYPER:
	case GICD_IIDR:
	case REG_RANGE(GICD_PIDR0, 4, 4):
	case REG_RANGE(GICD_PIDR4, 4, 4):
	case REG_RANGE(GICD_CIDR0, 4, 4):
		/* Allow read access, ignore write */
		if (!mmio->is_write)
			mmio_perform_access(gicd_base, mmio);
		/* fall through */
	default:
		/* Ignore access. */
		ret = MMIO_HANDLED;
	}

	return ret;
}

void irqchip_handle_irq(struct per_cpu *cpu_data)
{
	unsigned int count_event = 1;
	bool handled = false;
	u32 irq_id;

	while (1) {
		/* Read IAR1: set 'active' state */
		irq_id = gic_read_iar();

		if (irq_id == 0x3ff) /* Spurious IRQ */
			break;

		/* Handle IRQ */
		if (is_sgi(irq_id)) {
			arch_handle_sgi(cpu_data, irq_id, count_event);
			handled = true;
		} else {
			handled = arch_handle_phys_irq(cpu_data, irq_id,
						       count_event);
		}
		count_event = 0;

		/*
		 * Write EOIR1: drop priority, but stay active if handled is
		 * false.
		 * This allows to not be re-interrupted by a level-triggered
		 * interrupt that needs handling in the guest (e.g. timer)
		 */
		irqchip_eoi_irq(irq_id, handled);
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
	bool local_injection = (this_cpu_data() == cpu_data);
	unsigned int new_tail;

	if (!cpu_data) {
		/* Injection via GICD */
		mmio_write32(gicd_base + GICD_ISPENDR + (irq_id / 32) * 4,
			     1 << (irq_id % 32));
		return;
	}

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

	irqchip.cpu_reset(cpu_data);
}

void irqchip_cpu_shutdown(struct per_cpu *cpu_data)
{
	/*
	 * The GIC backend must take care of only resetting the hyp interface if
	 * it has been initialised: this function may be executed during the
	 * setup phase.
	 */
	irqchip.cpu_shutdown(cpu_data);
}

int irqchip_cell_init(struct cell *cell)
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
			     GICD_SIZE, gic_handle_dist_access, NULL);

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

void irqchip_cell_exit(struct cell *cell)
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

int irqchip_init(void)
{
	int i, err;
	u32 pidr2, cidr;
	u32 dev_id = 0;

	/* Only executed on master CPU */
	if (irqchip_is_init)
		return 0;

	gicd_base = paging_map_device(
			system_config->platform_info.arm.gicd_base, GICD_SIZE);
	if (!gicd_base)
		return -ENOMEM;

	for (i = 3; i >= 0; i--) {
		cidr = mmio_read32(gicd_base + GICD_CIDR0 + i * 4);
		dev_id |= cidr << i * 8;
	}
	if (dev_id != AMBA_DEVICE)
		return trace_error(-ENODEV);

	/* Probe the GIC version */
	pidr2 = mmio_read32(gicd_base + GICD_PIDR2);
	switch (GICD_PIDR2_ARCH(pidr2)) {
	case 0x2:
	case 0x3:
	case 0x4:
		break;
	default:
		return trace_error(-ENODEV);
	}

	err = irqchip.init();
	if (err)
		return err;

	irqchip_is_init = true;

	return 0;
}
