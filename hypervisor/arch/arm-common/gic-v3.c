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
#include <asm/gic_v3.h>
#include <asm/irqchip.h>
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
static unsigned int last_gicr;
static u32 gic_version;

static void *gicr_base;

static u64 gicv3_read_lr(unsigned int reg)
{
	u64 val;

	switch (reg) {
#define __READ_LR0_7(n)					\
	case n:						\
		ARM_GIC_READ_LR0_7(n, val)		\
		break;

	__READ_LR0_7(0)
	__READ_LR0_7(1)
	__READ_LR0_7(2)
	__READ_LR0_7(3)
	__READ_LR0_7(4)
	__READ_LR0_7(5)
	__READ_LR0_7(6)
	__READ_LR0_7(7)
#undef __READ_LR0_7

#define __READ_LR8_15(n)				\
	case n+8:					\
		ARM_GIC_READ_LR8_15(n, val)		\
		break;

	__READ_LR8_15(0)
	__READ_LR8_15(1)
	__READ_LR8_15(2)
	__READ_LR8_15(3)
	__READ_LR8_15(4)
	__READ_LR8_15(5)
	__READ_LR8_15(6)
	__READ_LR8_15(7)
#undef __READ_LR8_15

	default:
		return (u64)(-1);
	}

	return val;
}

static void gicv3_write_lr(unsigned int reg, u64 val)
{
	switch (reg) {
#define __WRITE_LR0_7(n)				\
	case n:						\
		ARM_GIC_WRITE_LR0_7(n, val)		\
		break;

	__WRITE_LR0_7(0)
	__WRITE_LR0_7(1)
	__WRITE_LR0_7(2)
	__WRITE_LR0_7(3)
	__WRITE_LR0_7(4)
	__WRITE_LR0_7(5)
	__WRITE_LR0_7(6)
	__WRITE_LR0_7(7)
#undef __WRITE_LR0_7

#define __WRITE_LR8_15(n)				\
	case n+8:					\
		ARM_GIC_WRITE_LR8_15(n, val)		\
		break;
	__WRITE_LR8_15(0)
	__WRITE_LR8_15(1)
	__WRITE_LR8_15(2)
	__WRITE_LR8_15(3)
	__WRITE_LR8_15(4)
	__WRITE_LR8_15(5)
	__WRITE_LR8_15(6)
	__WRITE_LR8_15(7)
#undef __WRITE_LR8_15
	}
}

static int gicv3_init(void)
{
	/* TODO: need to validate more? */
	if (!(mmio_read32(gicd_base + GICD_CTLR) & GICD_CTLR_ARE_NS))
		return trace_error(-EIO);

	/* Let the per-cpu code access the redistributors */
	gicr_base = paging_map_device(
			system_config->platform_info.arm.gicr_base, GICR_SIZE);
	if (!gicr_base)
		return -ENOMEM;

	last_gicr = system_config->root_cell.cpu_set_size * 8 - 1;
	while (!cpu_id_valid(last_gicr))
		last_gicr--;

	return 0;
}

static void gicv3_clear_pending_irqs(void)
{
	unsigned int n;

	/* Clear list registers. */
	for (n = 0; n < gic_num_lr; n++)
		gicv3_write_lr(n, 0);

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

static void gicv3_cpu_reset(struct per_cpu *cpu_data)
{
	unsigned int mnt_irq = system_config->platform_info.arm.maintenance_irq;
	void *gicr = cpu_data->public.gicr.base + GICR_SGI_BASE;

	gicv3_clear_pending_irqs();

	/* Ensure all IPIs and the maintenance PPI are enabled. */
	mmio_write32(gicr + GICR_ISENABLER, 0x0000ffff | (1 << mnt_irq));

	/* Disable PPIs, except for the maintenance interrupt. */
	mmio_write32(gicr + GICR_ICENABLER, 0xffff0000 & ~(1 << mnt_irq));

	/* Deactivate all active PPIs */
	mmio_write32(gicr + GICR_ICACTIVER, 0xffff0000);

	arm_write_sysreg(ICH_VMCR_EL2, 0);
}

static int gicv3_cpu_init(struct per_cpu *cpu_data)
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

	/* Probe the GICD version */
	gic_version = GICD_PIDR2_ARCH(mmio_read32(gicd_base + GICDv3_PIDR2));
	if (gic_version != 3 && gic_version != 4)
		return trace_error(-ENODEV);

	mpidr = cpu_data->public.mpidr;
	aff = (MPIDR_AFFINITY_LEVEL(mpidr, 3) << 24 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 0));

	/* Find redistributor */
	do {
		pidr = mmio_read32(redist_base + GICR_PIDR2);
		if (GICR_PIDR2_ARCH(pidr) != gic_version)
			break;

		redist_size = gic_version == 4 ? 0x40000 : 0x20000;

		typer = mmio_read64(redist_base + GICR_TYPER);
		if ((typer >> 32) == aff) {
			cpu_data->public.gicr.base = redist_base;
			cpu_data->public.gicr.phys_addr = redist_addr;
			break;
		}

		redist_base += redist_size;
		redist_addr += redist_size;
	} while (!(typer & GICR_TYPER_Last));

	if (!cpu_data->public.gicr.base) {
		printk("GIC: No redist found for CPU%d\n",
		       cpu_data->public.cpu_id);
		return -ENODEV;
	}

	/* Make sure we can handle Aff0 with the TargetList of ICC_SGI1R_EL1. */
	if ((cpu_data->public.mpidr & MPIDR_AFF0_MASK) >= 16)
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
	gicv3_clear_pending_irqs();

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

static int gicv3_cpu_shutdown(struct public_per_cpu *cpu_public)
{
	u32 ich_vmcr, icc_ctlr, cell_icc_igrpen1;

	if (!cpu_public->gicr.base)
		return -ENODEV;

	arm_write_sysreg(ICH_HCR_EL2, 0);

	/* Disable the maintenance interrupt - not used by Linux. */
	mmio_write32(cpu_public->gicr.base + GICR_SGI_BASE + GICR_ICENABLER,
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

	return 0;
}

static void gicv3_adjust_irq_target(struct cell *cell, u16 irq_id)
{
	void *irouter = gicd_base + GICD_IROUTER + 8 * irq_id;
	u64 mpidr = public_per_cpu(first_cpu(cell->cpu_set))->mpidr;
	u32 route = arm_cpu_by_mpidr(cell,
				     mmio_read64(irouter) & MPIDR_CPUID_MASK);

	if (!cell_owns_cpu(cell, route))
		mmio_write64(irouter, mpidr);
}

static enum mmio_result gicv3_handle_redist_access(void *arg,
						   struct mmio_access *mmio)
{
	struct public_per_cpu *cpu_public = arg;

	switch (mmio->address) {
	case GICR_TYPER:
		mmio_perform_access(cpu_public->gicr.base, mmio);
		if (cpu_public->cpu_id == last_gicr)
				mmio->value |= GICR_TYPER_Last;
		return MMIO_HANDLED;
	case GICR_IIDR:
	case 0xffd0 ... 0xfffc: /* ID registers */
		/*
		 * Read-only registers that might be used by a cell to find the
		 * redistributor corresponding to a CPU. Keep them accessible.
		 */
		break;
	case GICR_SYNCR:
		mmio->value = 0;
		return MMIO_HANDLED;
	case GICR_CTLR:
	case GICR_STATUSR:
	case GICR_WAKER:
	case GICR_SGI_BASE + GICR_ISENABLER:
	case GICR_SGI_BASE + GICR_ICENABLER:
	case GICR_SGI_BASE + GICR_ISPENDR:
	case GICR_SGI_BASE + GICR_ICPENDR:
	case GICR_SGI_BASE + GICR_ISACTIVER:
	case GICR_SGI_BASE + GICR_ICACTIVER:
	case REG_RANGE(GICR_SGI_BASE + GICR_IPRIORITYR, 8, 4):
	case REG_RANGE(GICR_SGI_BASE + GICR_ICFGR, 2, 4):
		if (this_cell() != cpu_public->cell) {
			/* ignore access to foreign redistributors */
			return MMIO_HANDLED;
		}
		break;
	default:
		/* ignore access */
		return MMIO_HANDLED;
	}

	mmio_perform_access(cpu_public->gicr.base, mmio);

	return MMIO_HANDLED;
}

static int gicv3_cell_init(struct cell *cell)
{
	unsigned int cpu;

	/*
	 * We register all regions so that the cell can iterate over the
	 * original range in order to find corresponding redistributors.
	 */
	for (cpu = 0; cpu < system_config->root_cell.cpu_set_size * 8; cpu++) {
		if (!cpu_id_valid(cpu))
			continue;
		mmio_region_register(cell, public_per_cpu(cpu)->gicr.phys_addr,
				     gic_version == 4 ? 0x40000 : 0x20000,
				     gicv3_handle_redist_access,
				     public_per_cpu(cpu));
	}

	return 0;
}

#define MPIDR_TO_SGIR_AFFINITY(cluster_id, level) \
	(MPIDR_AFFINITY_LEVEL((cluster_id), (level)) \
	<< ICC_SGIR_AFF## level ##_SHIFT)

static int gicv3_send_sgi(struct sgi *sgi)
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

bool gicv3_handle_sgir_write(u64 sgir)
{
	struct sgi sgi;
	unsigned long routing_mode = !!(sgir & ICC_SGIR_ROUTING_BIT);

	if (gic_version < 3)
		return false;

	sgi.targets = sgir & ICC_SGIR_TARGET_MASK;
	sgi.routing_mode = routing_mode;
	sgi.cluster_id = (SGIR_TO_MPIDR_AFFINITY(sgir, 3)
		       | SGIR_TO_MPIDR_AFFINITY(sgir, 2)
		       | SGIR_TO_MPIDR_AFFINITY(sgir, 1));
	sgi.id = sgir >> ICC_SGIR_IRQN_SHIFT & 0xf;

	gic_handle_sgir_write(&sgi);

	return true;
}

/*
 * GICv3 uses a 64bit register IROUTER for each IRQ
 */
static enum mmio_result gicv3_handle_irq_route(struct mmio_access *mmio,
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
			if ((public_per_cpu(cpu)->mpidr & MPIDR_CPUID_MASK) ==
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

static u32 gicv3_read_iar_irqn(void)
{
	u32 iar;

	arm_read_sysreg(ICC_IAR1_EL1, iar);
	return iar & 0xffffff;
}

static void gicv3_eoi_irq(u32 irq_id, bool deactivate)
{
	arm_write_sysreg(ICC_EOIR1_EL1, irq_id);
	if (deactivate)
		arm_write_sysreg(ICC_DIR_EL1, irq_id);
}

static int gicv3_inject_irq(u16 irq_id, u16 sender)
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
		lr = gicv3_read_lr(i);

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
	/* GICv3 doesn't support the injection of the calling CPU ID */

	gicv3_write_lr(free_lr, lr);

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
		if (gicv3_read_lr(n) & ICH_LR_PENDING)
			return true;

	return false;
}

static int gicv3_get_pending_irq(void)
{
	unsigned int n;
	u64 lr;

	for (n = 0; n < gic_num_lr; n++) {
		lr = gicv3_read_lr(n);
		if (lr & ICH_LR_PENDING) {
			gicv3_write_lr(n, 0);
			return (u32)lr;
		}
	}

	return -ENOENT;
}

static void gicv3_inject_phys_irq(u16 irq_id)
{
	void *gicr = this_cpu_public()->gicr.base + GICR_SGI_BASE;
	unsigned int offset = (irq_id / 32) * 4;
	unsigned int mask = 1 << (irq_id % 32);

	if (!is_spi(irq_id)) {
		/*
		 * Hardware interrupts are physically active until they are
		 * processed by the cell. Deactivate them first so that we can
		 * reinject.
		 * For simplicity reasons, we also issue deactivation for SGIs
		 * although they don't need this.
		 */
		mmio_write32(gicr + GICR_ICACTIVER, mask);

		/* inject via GICR */
		mmio_write32(gicr + GICR_ISPENDR, mask);
	} else {
		/* see above */
		mmio_write32(gicd_base + GICD_ICACTIVER + offset, mask);

		/* injet via GICD */
		mmio_write32(gicd_base + GICD_ISPENDR + offset, mask);
	}
}

static enum mmio_result gicv3_handle_irq_target(struct mmio_access *mmio,
						unsigned int irq)
{
	/* ignore writes, we are in affinity routing mode */
	return MMIO_HANDLED;
}

static enum mmio_result gicv3_handle_dist_access(struct mmio_access *mmio)
{
	switch (mmio->address) {
	case GICD_CTLR:
	case GICD_TYPER:
	case GICD_IIDR:
	case REG_RANGE(GICDv3_PIDR0, 4, 4):
	case REG_RANGE(GICDv3_PIDR4, 4, 4):
	case REG_RANGE(GICDv3_CIDR0, 4, 4):
		/* Allow read access, ignore write */
		if (!mmio->is_write)
			mmio_perform_access(gicd_base, mmio);
		/* fall through */
	default:
		/* Ignore access. */
		return MMIO_HANDLED;
	}
}

static int gicv3_get_cpu_target(unsigned int cpu_id)
{
	return 1 << public_per_cpu(cpu_id)->mpidr & MPIDR_AFF0_MASK;
}

static u64 gicv3_get_cluster_target(unsigned int cpu_id)
{
	return public_per_cpu(cpu_id)->mpidr & MPIDR_CLUSTERID_MASK;
}

const struct irqchip gicv3_irqchip = {
	.init = gicv3_init,
	.cpu_init = gicv3_cpu_init,
	.cpu_reset = gicv3_cpu_reset,
	.cpu_shutdown = gicv3_cpu_shutdown,
	.cell_init = gicv3_cell_init,
	.adjust_irq_target = gicv3_adjust_irq_target,
	.send_sgi = gicv3_send_sgi,
	.inject_irq = gicv3_inject_irq,
	.enable_maint_irq = gicv3_enable_maint_irq,
	.has_pending_irqs = gicv3_has_pending_irqs,
	.get_pending_irq = gicv3_get_pending_irq,
	.inject_phys_irq = gicv3_inject_phys_irq,
	.read_iar_irqn = gicv3_read_iar_irqn,
	.eoi_irq = gicv3_eoi_irq,
	.handle_irq_route = gicv3_handle_irq_route,
	.handle_irq_target = gicv3_handle_irq_target,
	.handle_dist_access = gicv3_handle_dist_access,
	.get_cpu_target = gicv3_get_cpu_target,
	.get_cluster_target = gicv3_get_cluster_target,

	.gicd_size = 0x10000,
};
