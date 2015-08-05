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
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <jailhouse/types.h>
#include <asm/control.h>
#include <asm/gic_common.h>
#include <asm/irqchip.h>
#include <asm/platform.h>
#include <asm/setup.h>
#include <asm/traps.h>

/*
 * This implementation assumes that the kernel driver already initialised most
 * of the GIC.
 * There is almost no instruction barrier, since IRQs are always disabled in the
 * hyp, and ERET serves as the context synchronization event.
 */

static unsigned int gic_num_lr;
static unsigned int gic_num_priority_bits;
static u32 gic_version;

extern void *gicd_base;
extern unsigned int gicd_size;
static void *gicr_base;
static unsigned int gicr_size;

static int gic_init(void)
{
	int err;

	/* FIXME: parse a dt */
	gicr_base = GICR_BASE;
	gicr_size = GICR_SIZE;

	/* Let the per-cpu code access the redistributors */
	err = arch_map_device(gicr_base, gicr_base, gicr_size);

	return err;
}

static void gic_clear_pending_irqs(void)
{
	unsigned int n;

	/* Clear list registers. */
	for (n = 0; n < gic_num_lr; n++)
		gic_write_lr(n, 0);

	/* Clear active priority bits */
	if (gic_num_priority_bits >= 5)
		arm_write_sysreg(ICH_AP1R0_EL2, 0);
	if (gic_num_priority_bits >= 6)
		arm_write_sysreg(ICH_AP1R1_EL2, 0);
	if (gic_num_priority_bits > 6) {
		arm_write_sysreg(ICH_AP1R2_EL2, 0);
		arm_write_sysreg(ICH_AP1R3_EL2, 0);
	}
}

static int gic_cpu_reset(struct per_cpu *cpu_data, bool is_shutdown)
{
	unsigned int i;
	void *gicr = cpu_data->gicr_base;
	unsigned long active;
	bool root_shutdown = is_shutdown && (cpu_data->cell == &root_cell);
	u32 ich_vmcr;

	if (gicr == 0)
		return -ENODEV;

	gic_clear_pending_irqs();

	gicr += GICR_SGI_BASE;
	active = mmio_read32(gicr + GICR_ICACTIVER);
	/* Deactivate all active PPIs */
	for (i = 16; i < 32; i++) {
		if (test_bit(i, &active))
			arm_write_sysreg(ICC_DIR_EL1, i);
	}

	/*
	 * Disable all PPIs, ensure IPIs are enabled.
	 * On shutdown, the root cell expects to find all its PPIs still enabled
	 * when returning to the driver.
	 */
	if (!root_shutdown)
		mmio_write32(gicr + GICR_ICENABLER, 0xffff0000);
	mmio_write32(gicr + GICR_ISENABLER, 0x0000ffff);

	if (root_shutdown) {
		/* Restore the root config */
		arm_read_sysreg(ICH_VMCR_EL2, ich_vmcr);

		if (!(ich_vmcr & ICH_VMCR_VEOIM)) {
			u32 icc_ctlr;
			arm_read_sysreg(ICC_CTLR_EL1, icc_ctlr);
			icc_ctlr &= ~ICC_CTLR_EOImode;
			arm_write_sysreg(ICC_CTLR_EL1, icc_ctlr);
		}

		arm_write_sysreg(ICH_HCR_EL2, 0);
	}

	arm_write_sysreg(ICH_VMCR_EL2, 0);

	return 0;
}

static int gic_cpu_init(struct per_cpu *cpu_data)
{
	u64 typer;
	u32 pidr;
	u32 cell_icc_ctlr, cell_icc_pmr, cell_icc_igrpen1;
	u32 ich_vtr;
	u32 ich_vmcr;
	void *redist_base = gicr_base;

	/* Find redistributor */
	do {
		pidr = mmio_read32(redist_base + GICR_PIDR2);
		gic_version = GICR_PIDR2_ARCH(pidr);
		if (gic_version != 3 && gic_version != 4)
			break;

		typer = mmio_read64(redist_base + GICR_TYPER);
		if ((typer >> 32) == cpu_data->cpu_id) {
			cpu_data->gicr_base = redist_base;
			break;
		}

		redist_base += 0x20000;
		if (gic_version == 4)
			redist_base += 0x20000;
	} while (!(typer & GICR_TYPER_Last));

	if (cpu_data->gicr_base == 0) {
		printk("GIC: No redist found for CPU%d\n", cpu_data->cpu_id);
		return -ENODEV;
	}

	/* Ensure all IPIs are enabled */
	mmio_write32(redist_base + GICR_SGI_BASE + GICR_ISENABLER, 0x0000ffff);

	/*
	 * Set EOIMode to 1
	 * This allow to drop the priority of level-triggered interrupts without
	 * deactivating them, and thus ensure that they won't be immediately
	 * re-triggered. (e.g. timer)
	 * They can then be injected into the guest using the LR.HW bit, and
	 * will be deactivated once the guest does an EOI after handling the
	 * interrupt source.
	 */
	arm_read_sysreg(ICC_CTLR_EL1, cell_icc_ctlr);
	arm_write_sysreg(ICC_CTLR_EL1, ICC_CTLR_EOImode);

	arm_read_sysreg(ICC_PMR_EL1, cell_icc_pmr);
	arm_write_sysreg(ICC_PMR_EL1, ICC_PMR_DEFAULT);

	arm_read_sysreg(ICC_IGRPEN1_EL1, cell_icc_igrpen1);
	arm_write_sysreg(ICC_IGRPEN1_EL1, ICC_IGRPEN1_EN);

	arm_read_sysreg(ICH_VTR_EL2, ich_vtr);
	gic_num_lr = (ich_vtr & 0xf) + 1;
	gic_num_priority_bits = (ich_vtr >> 29) + 1;

	/*
	 * Clear pending virtual IRQs in case anything is left from previous
	 * use. Physically pending IRQs will be forwarded to Linux once we
	 * enable interrupts for the hypervisor.
	 */
	gic_clear_pending_irqs();

	ich_vmcr = (cell_icc_pmr & ICC_PMR_MASK) << ICH_VMCR_VPMR_SHIFT;
	if (cell_icc_igrpen1 & ICC_IGRPEN1_EN)
		ich_vmcr |= ICH_VMCR_VENG1;
	if (cell_icc_ctlr & ICC_CTLR_EOImode)
		ich_vmcr |= ICH_VMCR_VEOIM;
	arm_write_sysreg(ICH_VMCR_EL2, ich_vmcr);

	/* After this, the cells access the virtual interface of the GIC. */
	arm_write_sysreg(ICH_HCR_EL2, ICH_HCR_EN);

	return 0;
}

static void gic_route_spis(struct cell *config_cell, struct cell *dest_cell)
{
	int i;
	void *irouter = gicd_base + GICD_IROUTER;
	unsigned int first_cpu;

	/* Use the core functions to retrieve the first physical id */
	for_each_cpu(first_cpu, dest_cell->cpu_set)
		break;

	for (i = 0; i < 64; i++, irouter += 8) {
		if (spi_in_cell(config_cell, i))
			mmio_write64(irouter, first_cpu);
	}
}

static int gic_cell_init(struct cell *cell)
{
	gic_route_spis(cell, cell);
	return 0;
}

static void gic_cell_exit(struct cell *cell)
{
	/* Reset interrupt routing of the cell's spis*/
	gic_route_spis(cell, &root_cell);
}

static int gic_send_sgi(struct sgi *sgi)
{
	u64 val;
	u16 targets = sgi->targets;

	if (!is_sgi(sgi->id))
		return -EINVAL;

	if (sgi->routing_mode == 2)
		targets = 1 << phys_processor_id();

	val = (u64)sgi->aff3 << ICC_SGIR_AFF3_SHIFT
	    | (u64)sgi->aff2 << ICC_SGIR_AFF2_SHIFT
	    | sgi->aff1 << ICC_SGIR_AFF1_SHIFT
	    | (targets & ICC_SGIR_TARGET_MASK)
	    | (sgi->id & 0xf) << ICC_SGIR_IRQN_SHIFT;

	if (sgi->routing_mode == 1)
		val |= ICC_SGIR_ROUTING_BIT;

	/*
	 * Ensure the targets see our modifications to their per-cpu
	 * structures.
	 */
	dsb(ish);

	arm_write_sysreg(ICC_SGI1R_EL1, val);
	isb();

	return 0;
}

int gicv3_handle_sgir_write(struct per_cpu *cpu_data, u64 sgir)
{
	struct sgi sgi;
	unsigned long routing_mode = !!(sgir & ICC_SGIR_ROUTING_BIT);

	/* FIXME: clusters are not supported yet. */
	sgi.targets = sgir & ICC_SGIR_TARGET_MASK;
	sgi.routing_mode = routing_mode;
	sgi.aff1 = sgir >> ICC_SGIR_AFF1_SHIFT & 0xff;
	sgi.aff2 = sgir >> ICC_SGIR_AFF2_SHIFT & 0xff;
	sgi.aff3 = sgir >> ICC_SGIR_AFF3_SHIFT & 0xff;
	sgi.id = sgir >> ICC_SGIR_IRQN_SHIFT & 0xf;

	return gic_handle_sgir_write(cpu_data, &sgi, true);
}

static void gic_eoi_irq(u32 irq_id, bool deactivate)
{
	arm_write_sysreg(ICC_EOIR1_EL1, irq_id);
	if (deactivate)
		arm_write_sysreg(ICC_DIR_EL1, irq_id);
}

static int gic_inject_irq(struct per_cpu *cpu_data, struct pending_irq *irq)
{
	int i;
	int free_lr = -1;
	u32 elsr;
	u64 lr;

	arm_read_sysreg(ICH_ELSR_EL2, elsr);
	for (i = 0; i < gic_num_lr; i++) {
		if ((elsr >> i) & 1) {
			/* Entry is invalid, candidate for injection */
			if (free_lr == -1)
				free_lr = i;
			continue;
		}

		/*
		 * Entry is in use, check that it doesn't match the one we want
		 * to inject.
		 */
		lr = gic_read_lr(i);

		/*
		 * A strict phys->virt id mapping is used for SPIs, so this test
		 * should be sufficient.
		 */
		if ((u32)lr == irq->virt_id)
			return -EINVAL;
	}

	if (free_lr == -1) {
		u32 hcr;
		/*
		 * All list registers are in use, trigger a maintenance
		 * interrupt once they are available again.
		 */
		arm_read_sysreg(ICH_HCR_EL2, hcr);
		hcr |= ICH_HCR_UIE;
		arm_write_sysreg(ICH_HCR_EL2, hcr);

		return -EBUSY;
	}

	lr = irq->virt_id;
	/* Only group 1 interrupts */
	lr |= ICH_LR_GROUP_BIT;
	lr |= ICH_LR_PENDING;
	if (irq->hw) {
		lr |= ICH_LR_HW_BIT;
		lr |= (u64)irq->type.irq << ICH_LR_PHYS_ID_SHIFT;
	} else if (irq->type.sgi.maintenance) {
		lr |= ICH_LR_SGI_EOI;
	}

	gic_write_lr(free_lr, lr);

	return 0;
}

static int gic_handle_redist_access(struct per_cpu *cpu_data,
				    struct mmio_access *access)
{
	unsigned int cpu;
	unsigned int reg;
	int ret = TRAP_UNHANDLED;
	unsigned int virt_id;
	void *virt_redist = 0;
	void *phys_redist = 0;
	unsigned int redist_size = (gic_version == 4) ? 0x40000 : 0x20000;
	void *address = (void *)access->addr;

	/*
	 * The redistributor accessed by the cell is not the one stored in these
	 * cpu_datas, but the one associated to its virtual id. So we first
	 * need to translate the redistributor address.
	 */
	for_each_cpu(cpu, cpu_data->cell->cpu_set) {
		virt_id = arm_cpu_phys2virt(cpu);
		virt_redist = per_cpu(virt_id)->gicr_base;
		if (address >= virt_redist && address < virt_redist
				+ redist_size) {
			phys_redist = per_cpu(cpu)->gicr_base;
			break;
		}
	}

	if (phys_redist == NULL)
		return TRAP_FORBIDDEN;

	reg = address - virt_redist;
	access->addr = (unsigned long)phys_redist + reg;

	/* Change the ID register, all other accesses are allowed. */
	if (!access->is_write) {
		switch (reg) {
		case GICR_TYPER:
			if (virt_id == cpu_data->cell->arch.last_virt_id)
				access->val = GICR_TYPER_Last;
			else
				access->val = 0;
			/* AArch64 can use a writeq for this register */
			if (access->size == 8)
				access->val |= (u64)virt_id << 32;

			ret = TRAP_HANDLED;
			break;
		case GICR_TYPER + 4:
			/* Upper bits contain the affinity */
			access->val = virt_id;
			ret = TRAP_HANDLED;
			break;
		}
	}
	if (ret == TRAP_HANDLED)
		return ret;

	arm_mmio_perform_access(access);
	return TRAP_HANDLED;
}

static int gic_mmio_access(struct per_cpu *cpu_data,
			   struct mmio_access *access)
{
	void *address = (void *)access->addr;

	if (address >= gicd_base && address < gicd_base + gicd_size)
		return gic_handle_dist_access(cpu_data, access);

	if (address >= gicr_base && address < gicr_base + gicr_size)
		return gic_handle_redist_access(cpu_data, access);

	return TRAP_UNHANDLED;
}

struct irqchip_ops gic_irqchip = {
	.init = gic_init,
	.cpu_init = gic_cpu_init,
	.cpu_reset = gic_cpu_reset,
	.cell_init = gic_cell_init,
	.cell_exit = gic_cell_exit,
	.send_sgi = gic_send_sgi,
	.handle_irq = gic_handle_irq,
	.inject_irq = gic_inject_irq,
	.eoi_irq = gic_eoi_irq,
	.mmio_access = gic_mmio_access,
};
