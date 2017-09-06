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
#include <asm/gic.h>
#include <asm/irqchip.h>
#include <asm/setup.h>
#include <asm/sysregs.h>
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

static void *gicr_base;

static int gic_init(void)
{
	/* TODO: need to validate more? */
	if (!(mmio_read32(gicd_base + GICD_CTLR) & GICD_CTLR_ARE_NS))
		return trace_error(-EIO);

	/* Let the per-cpu code access the redistributors */
	gicr_base = paging_map_device(
			system_config->platform_info.arm.gicr_base, GICR_SIZE);
	if (!gicr_base)
		return -ENOMEM;

	return 0;
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

static void gic_cpu_reset(struct per_cpu *cpu_data)
{
	unsigned int mnt_irq = system_config->platform_info.arm.maintenance_irq;
	void *gicr = cpu_data->gicr.base + GICR_SGI_BASE;

	gic_clear_pending_irqs();

	/* Ensure all IPIs and the maintenance PPI are enabled. */
	mmio_write32(gicr + GICR_ISENABLER, 0x0000ffff | (1 << mnt_irq));

	/* Disable PPIs, except for the maintenance interrupt. */
	mmio_write32(gicr + GICR_ICENABLER, 0xffff0000 & ~(1 << mnt_irq));

	/* Deactivate all active PPIs */
	mmio_write32(gicr + GICR_ICACTIVER, 0xffff0000);

	arm_write_sysreg(ICH_VMCR_EL2, 0);
}

static int gic_cpu_init(struct per_cpu *cpu_data)
{
	unsigned int mnt_irq = system_config->platform_info.arm.maintenance_irq;
	unsigned long redist_addr = system_config->platform_info.arm.gicr_base;
	void *redist_base = gicr_base;
	unsigned long redist_size;
	u64 typer, mpidr;
	u32 pidr, aff;
	u32 cell_icc_ctlr, cell_icc_pmr, cell_icc_igrpen1;
	u32 ich_vtr;
	u32 ich_vmcr;

	mpidr = cpu_data->mpidr;
	aff = (MPIDR_AFFINITY_LEVEL(mpidr, 3) << 24 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 0));

	/* Find redistributor */
	do {
		pidr = mmio_read32(redist_base + GICR_PIDR2);
		gic_version = GICR_PIDR2_ARCH(pidr);
		if (gic_version != 3 && gic_version != 4)
			break;

		redist_size = gic_version == 4 ? 0x40000 : 0x20000;

		typer = mmio_read64(redist_base + GICR_TYPER);
		if ((typer >> 32) == aff) {
			cpu_data->gicr.base = redist_base;
			cpu_data->gicr.phys_addr = redist_addr;
			break;
		}

		redist_base += redist_size;
		redist_addr += redist_size;
	} while (!(typer & GICR_TYPER_Last));

	if (!cpu_data->gicr.base) {
		printk("GIC: No redist found for CPU%d\n", cpu_data->cpu_id);
		return -ENODEV;
	}

	/* Make sure we can handle Aff0 with the TargetList of ICC_SGI1R_EL1. */
	if ((cpu_data->mpidr & MPIDR_AFF0_MASK) >= 16)
		return trace_error(-EIO);

	/* Ensure all IPIs and the maintenance PPI are enabled. */
	mmio_write32(redist_base + GICR_SGI_BASE + GICR_ISENABLER,
		     0x0000ffff | (1 << mnt_irq));

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

static void gic_cpu_shutdown(struct per_cpu *cpu_data)
{
	u32 ich_vmcr, icc_ctlr, cell_icc_igrpen1;

	if (!cpu_data->gicr.base)
		return;

	arm_write_sysreg(ICH_HCR_EL2, 0);

	/* Disable the maintenance interrupt - not used by Linux. */
	mmio_write32(cpu_data->gicr.base + GICR_SGI_BASE + GICR_ICENABLER,
		     1 << system_config->platform_info.arm.maintenance_irq);

	/* Restore the root config */
	arm_read_sysreg(ICH_VMCR_EL2, ich_vmcr);

	if (!(ich_vmcr & ICH_VMCR_VEOIM)) {
		arm_read_sysreg(ICC_CTLR_EL1, icc_ctlr);
		icc_ctlr &= ~ICC_CTLR_EOImode;
		arm_write_sysreg(ICC_CTLR_EL1, icc_ctlr);
	}
	if (!(ich_vmcr & ICH_VMCR_VENG1)) {
		arm_read_sysreg(ICC_IGRPEN1_EL1, cell_icc_igrpen1);
		cell_icc_igrpen1 &= ~ICC_IGRPEN1_EN;
		arm_write_sysreg(ICC_IGRPEN1_EL1, cell_icc_igrpen1);
	}
}

static void gic_adjust_irq_target(struct cell *cell, u16 irq_id)
{
	void *irouter = gicd_base + GICD_IROUTER + 8 * irq_id;
	u64 mpidr = per_cpu(first_cpu(cell->cpu_set))->mpidr;
	u32 route = arm_cpu_by_mpidr(cell,
				     mmio_read64(irouter) & MPIDR_CPUID_MASK);

	if (!cell_owns_cpu(cell, route))
		mmio_write64(irouter, mpidr);
}

static enum mmio_result gic_handle_redist_access(void *arg,
						 struct mmio_access *mmio)
{
	struct per_cpu *cpu_data = arg;

	mmio_perform_access(cpu_data->gicr.base, mmio);

	/*
	 * Declare each redistributor region to be last. This avoids that we
	 * miss one and cause the guest to overscan while matching
	 * redistributors in a partitioned region.
	 */
	if (mmio->address == GICR_TYPER && !mmio->is_write)
		mmio->value |= GICR_TYPER_Last;

	return MMIO_HANDLED;
}

static int gic_cell_init(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu(cpu, cell->cpu_set)
		mmio_region_register(cell, per_cpu(cpu)->gicr.phys_addr,
				     gic_version == 4 ? 0x40000 : 0x20000,
				     gic_handle_redist_access, per_cpu(cpu));

	return 0;
}

#define MPIDR_TO_SGIR_AFFINITY(cluster_id, level) \
	(MPIDR_AFFINITY_LEVEL((cluster_id), (level)) \
	<< ICC_SGIR_AFF## level ##_SHIFT)

static int gic_send_sgi(struct sgi *sgi)
{
	u64 val;
	u16 targets = sgi->targets;

	if (!is_sgi(sgi->id))
		return -EINVAL;

	if (sgi->routing_mode == 2)
		targets = 1 << phys_processor_id();

	val = (MPIDR_TO_SGIR_AFFINITY(sgi->cluster_id, 3)
	    | MPIDR_TO_SGIR_AFFINITY(sgi->cluster_id, 2)
	    | MPIDR_TO_SGIR_AFFINITY(sgi->cluster_id, 1)
	    | (targets & ICC_SGIR_TARGET_MASK)
	    | (sgi->id & 0xf) << ICC_SGIR_IRQN_SHIFT);

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

#define SGIR_TO_AFFINITY(sgir, level)	\
	((sgir) >> ICC_SGIR_AFF## level ##_SHIFT & 0xff)

#define SGIR_TO_MPIDR_AFFINITY(sgir, level)			\
	(SGIR_TO_AFFINITY(sgir, level) << MPIDR_LEVEL_SHIFT(level))

void gicv3_handle_sgir_write(u64 sgir)
{
	struct sgi sgi;
	unsigned long routing_mode = !!(sgir & ICC_SGIR_ROUTING_BIT);

	/* FIXME: clusters are not supported yet. */
	sgi.targets = sgir & ICC_SGIR_TARGET_MASK;
	sgi.routing_mode = routing_mode;
	sgi.cluster_id = (SGIR_TO_MPIDR_AFFINITY(sgir, 3)
		       | SGIR_TO_MPIDR_AFFINITY(sgir, 2)
		       | SGIR_TO_MPIDR_AFFINITY(sgir, 1));
	sgi.id = sgir >> ICC_SGIR_IRQN_SHIFT & 0xf;

	gic_handle_sgir_write(&sgi, true);
}

/*
 * GICv3 uses a 64bit register IROUTER for each IRQ
 */
enum mmio_result gic_handle_irq_route(struct mmio_access *mmio,
				      unsigned int irq)
{
	struct cell *cell = this_cell();
	unsigned int cpu;

	/* Ignore aff3 on AArch32 (return 0) */
	if (mmio->size == 4 && (mmio->address % 8))
		return MMIO_HANDLED;

	/* SGIs and PPIs are res0 */
	if (!is_spi(irq))
		return MMIO_HANDLED;

	/*
	 * Ignore accesses to SPIs that do not belong to the cell. This isn't
	 * forbidden, because the guest driver may simply iterate over all
	 * registers at initialisation
	 */
	if (!irqchip_irq_in_cell(cell, irq))
		return MMIO_HANDLED;

	if (mmio->is_write) {
		/*
		 * Validate that the target CPU is part of the cell.
		 * Note that we do not support Interrupt Routing Mode = 1.
		 */
		for_each_cpu(cpu, cell->cpu_set)
			if ((per_cpu(cpu)->mpidr & MPIDR_CPUID_MASK) ==
			    mmio->value) {
				mmio_perform_access(gicd_base, mmio);
				return MMIO_HANDLED;
			}

		printk("Attempt to route IRQ%d outside of cell\n", irq);
		return MMIO_ERROR;
	} else {
		mmio->value = mmio_read64(gicd_base + GICD_IROUTER + 8 * irq);
		return MMIO_HANDLED;
	}
}

static void gic_eoi_irq(u32 irq_id, bool deactivate)
{
	arm_write_sysreg(ICC_EOIR1_EL1, irq_id);
	if (deactivate)
		arm_write_sysreg(ICC_DIR_EL1, irq_id);
}

static int gic_inject_irq(struct per_cpu *cpu_data, u16 irq_id)
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
		if ((u32)lr == irq_id)
			return -EEXIST;
	}

	if (free_lr == -1)
		/* All list registers are in use */
		return -EBUSY;

	lr = irq_id;
	/* Only group 1 interrupts */
	lr |= ICH_LR_GROUP_BIT;
	lr |= ICH_LR_PENDING;
	if (!is_sgi(irq_id)) {
		lr |= ICH_LR_HW_BIT;
		lr |= (u64)irq_id << ICH_LR_PHYS_ID_SHIFT;
	}

	gic_write_lr(free_lr, lr);

	return 0;
}

static void gicv3_enable_maint_irq(bool enable)
{
	u32 hcr;

	arm_read_sysreg(ICH_HCR_EL2, hcr);
	if (enable)
		hcr |= ICH_HCR_UIE;
	else
		hcr &= ~ICH_HCR_UIE;
	arm_write_sysreg(ICH_HCR_EL2, hcr);
}

static bool gicv3_has_pending_irqs(void)
{
	unsigned int n;

	for (n = 0; n < gic_num_lr; n++)
		if (gic_read_lr(n) & ICH_LR_PENDING)
			return true;

	return false;
}

static enum mmio_result gicv3_handle_irq_target(struct mmio_access *mmio,
						unsigned int irq)
{
	/* ignore writes, we are in affinity routing mode */
	return MMIO_HANDLED;
}

unsigned int irqchip_mmio_count_regions(struct cell *cell)
{
	unsigned int cpu, regions = 1; /* GICD */

	/* 1 GICR per CPU */
	for_each_cpu(cpu, cell->cpu_set)
		regions++;

	return regions;
}

struct irqchip_ops irqchip = {
	.init = gic_init,
	.cpu_init = gic_cpu_init,
	.cpu_reset = gic_cpu_reset,
	.cpu_shutdown = gic_cpu_shutdown,
	.cell_init = gic_cell_init,
	.adjust_irq_target = gic_adjust_irq_target,
	.send_sgi = gic_send_sgi,
	.inject_irq = gic_inject_irq,
	.enable_maint_irq = gicv3_enable_maint_irq,
	.has_pending_irqs = gicv3_has_pending_irqs,
	.eoi_irq = gic_eoi_irq,
	.handle_irq_target = gicv3_handle_irq_target,
};
