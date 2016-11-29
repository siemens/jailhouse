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

#include <jailhouse/cell.h>
#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <asm/control.h>
#include <asm/gic.h>
#include <asm/irqchip.h>
#include <asm/percpu.h>
#include <asm/spinlock.h>
#include <asm/traps.h>

#define REG_RANGE(base, n, size)		\
		(base) ... ((base) + (n - 1) * (size))

static DEFINE_SPINLOCK(dist_lock);

/* The GIC interface numbering does not necessarily match the logical map */
u8 target_cpu_map[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* Check that the targeted interface belongs to the cell */
bool gic_targets_in_cell(struct cell *cell, u8 targets)
{
	unsigned int cpu;

	for (cpu = 0; cpu < ARRAY_SIZE(target_cpu_map); cpu++)
		if (targets & target_cpu_map[cpu] &&
		    per_cpu(cpu)->cell != cell)
			return false;

	return true;
}

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

/*
 * GICv2 uses 8bit values for each IRQ in the ITARGETRs registers
 */
static enum mmio_result handle_irq_target(struct mmio_access *mmio,
					  unsigned int irq)
{
	/*
	 * ITARGETSR contain one byte per IRQ, so the first one affected by this
	 * access corresponds to the reg index
	 */
	unsigned int irq_base = irq & ~0x3;
	struct cell *cell = this_cell();
	unsigned int offset;
	u32 access_mask = 0;
	unsigned int n;
	u8 targets;

	/*
	 * Let the guest freely access its SGIs and PPIs, which may be used to
	 * fill its CPU interface map.
	 */
	if (!is_spi(irq)) {
		mmio_perform_access(gicd_base, mmio);
		return MMIO_HANDLED;
	}

	/*
	 * The registers are byte-accessible, but we always do word accesses.
	 */
	offset = irq % 4;
	mmio->address &= ~0x3;
	mmio->value <<= 8 * offset;
	mmio->size = 4;

	for (n = 0; n < 4; n++) {
		if (irqchip_irq_in_cell(cell, irq_base + n))
			access_mask |= 0xff << (8 * n);
		else
			continue;

		if (!mmio->is_write)
			continue;

		targets = (mmio->value >> (8 * n)) & 0xff;

		if (!gic_targets_in_cell(cell, targets)) {
			printk("Attempt to route IRQ%d outside of cell\n",
			       irq_base + n);
			return MMIO_ERROR;
		}
	}

	if (mmio->is_write) {
		spin_lock(&dist_lock);
		u32 itargetsr =
			mmio_read32(gicd_base + GICD_ITARGETSR + irq_base);
		mmio->value &= access_mask;
		/* Combine with external SPIs */
		mmio->value |= (itargetsr & ~access_mask);
		/* And do the access */
		mmio_perform_access(gicd_base, mmio);
		spin_unlock(&dist_lock);
	} else {
		mmio_perform_access(gicd_base, mmio);
		mmio->value &= access_mask;
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

/*
 * Get the CPU interface ID for this cpu. It can be discovered by reading
 * the banked value of the PPI and IPI TARGET registers
 * Patch 2bb3135 in Linux explains why the probe may need to scans the first 8
 * registers: some early implementation returned 0 for the first ITARGETSR
 * registers.
 * Since those didn't have virtualization extensions, we can safely ignore that
 * case.
 */
int gic_probe_cpu_id(unsigned int cpu)
{
	if (cpu >= ARRAY_SIZE(target_cpu_map))
		return -EINVAL;

	target_cpu_map[cpu] = mmio_read32(gicd_base + GICD_ITARGETSR);

	if (target_cpu_map[cpu] == 0)
		return -ENODEV;

	return 0;
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
				if (!(targets & target_cpu_map[cpu]))
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

enum mmio_result gic_handle_dist_access(void *arg, struct mmio_access *mmio)
{
	unsigned long reg = mmio->address;
	enum mmio_result ret;

	switch (reg) {
	case REG_RANGE(GICD_IROUTER, 1024, 8):
		ret = gic_handle_irq_route(mmio, (reg - GICD_IROUTER) / 8);
		break;

	case REG_RANGE(GICD_ITARGETSR, 1024, 1):
		ret = handle_irq_target(mmio, reg - GICD_ITARGETSR);
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

void gic_set_irq_pending(u16 irq_id)
{
	mmio_write32(gicd_base + GICD_ISPENDR + (irq_id / 32) * 4,
		     1 << (irq_id % 32));
}
