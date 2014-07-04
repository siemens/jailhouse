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

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <asm/cell.h>
#include <asm/gic_common.h>
#include <asm/irqchip.h>
#include <asm/percpu.h>
#include <asm/platform.h>
#include <asm/spinlock.h>
#include <asm/traps.h>

#define REG_RANGE(base, n, size)		\
		(base) ... ((base) + (n - 1) * (size))

extern void *gicd_base;
extern unsigned int gicd_size;

static DEFINE_SPINLOCK(dist_lock);

/*
 * Most of the GIC distributor writes only reconfigure the IRQs corresponding to
 * the bits of the written value, by using separate `set' and `clear' registers.
 * Such registers can be handled by setting the `is_poke' boolean, which allows
 * to simply restrict the access->val with the cell configuration mask.
 * Others, such as the priority registers, will need to be read and written back
 * with a restricted value, by using the distributor lock.
 */
static int restrict_bitmask_access(struct per_cpu *cpu_data,
				   struct mmio_access *access,
				   unsigned int reg_index,
				   unsigned int bits_per_irq,
				   bool is_poke)
{
	unsigned int spi;
	unsigned long access_mask = 0;
	/*
	 * In order to avoid division, the number of bits per irq is limited
	 * to powers of 2 for the moment.
	 */
	unsigned long irqs_per_reg = 32 >> ffsl(bits_per_irq);
	unsigned long spi_bits = (1 << bits_per_irq) - 1;
	/* First, extract the first interrupt affected by this access */
	unsigned int first_irq = reg_index * irqs_per_reg;

	/* For SGIs or PPIs, let the caller do the mmio access */
	if (!is_spi(first_irq))
		return TRAP_UNHANDLED;

	/* For SPIs, compare against the cell config mask */
	first_irq -= 32;
	for (spi = first_irq; spi < first_irq + irqs_per_reg; spi++) {
		unsigned int bit_nr = (spi - first_irq) * bits_per_irq;
		if (spi_in_cell(cpu_data->cell, spi))
			access_mask |= spi_bits << bit_nr;
	}

	if (!access->is_write) {
		/* Restrict the read value */
		arch_mmio_access(access);
		access->val &= access_mask;
		return TRAP_HANDLED;
	}

	if (!is_poke) {
		/*
		 * Modify the existing value of this register by first reading
		 * it into access->val
		 * Relies on a spinlock since we need two mmio accesses.
		 */
		unsigned long access_val = access->val;

		spin_lock(&dist_lock);

		access->is_write = false;
		arch_mmio_access(access);
		access->is_write = true;

		/* Clear 0 bits */
		access->val &= ~(access_mask & ~access_val);
		access->val |= access_val;
		arch_mmio_access(access);

		spin_unlock(&dist_lock);

		return TRAP_HANDLED;
	} else {
		access->val &= access_mask;
		/* Do the access */
		return TRAP_UNHANDLED;
	}
}

/*
 * GICv3 uses a 64bit register IROUTER for each IRQ
 */
static int handle_irq_route(struct per_cpu *cpu_data,
			    struct mmio_access *access, unsigned int irq)
{
	struct cell *cell = cpu_data->cell;
	unsigned int cpu;

	/* Ignore aff3 on AArch32 (return 0) */
	if (access->size == 4 && (access->addr % 8))
		return TRAP_HANDLED;

	/* SGIs and PPIs are res0 */
	if (!is_spi(irq))
		return TRAP_HANDLED;

	/*
	 * Ignore accesses to SPIs that do not belong to the cell. This isn't
	 * forbidden, because the guest driver may simply iterate over all
	 * registers at initialisation
	 */
	if (!spi_in_cell(cell, irq - 32))
		return TRAP_HANDLED;

	/* Translate the virtual cpu id into the physical one */
	if (access->is_write) {
		access->val = arm_cpu_virt2phys(cell, access->val);
		if (access->val == -1) {
			printk("Attempt to route IRQ%d outside of cell\n", irq);
			return TRAP_FORBIDDEN;
		}
		/* And do the access */
		return TRAP_UNHANDLED;
	} else {
		cpu = mmio_read32(gicd_base + GICD_IROUTER + 8 * irq);
		access->val = arm_cpu_phys2virt(cpu);
		return TRAP_HANDLED;
	}
}

int gic_handle_dist_access(struct per_cpu *cpu_data,
			   struct mmio_access *access)
{
	int ret;
	unsigned long reg = access->addr - (unsigned long)gicd_base;

	switch (reg) {
	case REG_RANGE(GICD_IROUTER, 1024, 8):
		ret = handle_irq_route(cpu_data, access,
				(reg - GICD_IROUTER) / 8);
		break;

	case REG_RANGE(GICD_ICENABLER, 32, 4):
	case REG_RANGE(GICD_ISENABLER, 32, 4):
	case REG_RANGE(GICD_ICPENDR, 32, 4):
	case REG_RANGE(GICD_ISPENDR, 32, 4):
	case REG_RANGE(GICD_ICACTIVER, 32, 4):
	case REG_RANGE(GICD_ISACTIVER, 32, 4):
		ret = restrict_bitmask_access(cpu_data, access,
				(reg & 0x7f) / 4, 1, true);
		break;

	case REG_RANGE(GICD_IGROUPR, 32, 4):
		ret = restrict_bitmask_access(cpu_data, access,
				(reg & 0x7f) / 4, 1, false);
		break;

	case REG_RANGE(GICD_ICFGR, 64, 4):
		ret = restrict_bitmask_access(cpu_data, access,
				(reg & 0xff) / 4, 2, false);
		break;

	case REG_RANGE(GICD_IPRIORITYR, 255, 4):
		ret = restrict_bitmask_access(cpu_data, access,
				(reg & 0x3ff) / 4, 8, false);
		break;

	case GICD_CTLR:
	case GICD_TYPER:
	case GICD_IIDR:
	case REG_RANGE(GICD_PIDR0, 4, 4):
	case REG_RANGE(GICD_PIDR4, 4, 4):
	case REG_RANGE(GICD_CIDR0, 4, 4):
		/* Allow read access, ignore write */
		ret = (access->is_write ? TRAP_HANDLED : TRAP_UNHANDLED);
		break;

	default:
		/* Ignore access. */
		ret = TRAP_HANDLED;
	}

	/* The sub-handlers return TRAP_UNHANDLED to allow the access */
	if (ret == TRAP_UNHANDLED) {
		arch_mmio_access(access);
		ret = TRAP_HANDLED;
	}

	return ret;
}
