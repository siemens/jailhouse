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
	void *gicr = cpu_data->gicr_base + GICR_SGI_BASE;

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

		redist_base += gic_version == 4 ? 0x40000 : 0x20000;
	} while (!(typer & GICR_TYPER_Last));

	if (!cpu_data->gicr_base) {
		printk("GIC: No redist found for CPU%d\n", cpu_data->cpu_id);
		return -ENODEV;
	}

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

	if (!cpu_data->gicr_base)
		return;

	arm_write_sysreg(ICH_HCR_EL2, 0);

	/* Disable the maintenance interrupt - not used by Linux. */
	mmio_write32(cpu_data->gicr_base + GICR_SGI_BASE + GICR_ICENABLER,
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
	void *irouter = gicd_base + GICD_IROUTER + irq_id;
	u32 route = mmio_read32(irouter);

	if (!cell_owns_cpu(cell, route))
		mmio_write32(irouter, first_cpu(cell->cpu_set));
}

static enum mmio_result gic_handle_redist_access(void *arg,
						 struct mmio_access *mmio)
{
	struct cell *cell = this_cell();
	unsigned int cpu;
	unsigned int virt_id;
	unsigned int redist_size = (gic_version == 4) ? 0x40000 : 0x20000;
	void *phys_redist = NULL;
	unsigned long offs;

	/*
	 * The redistributor accessed by the cell is not the one stored in these
	 * cpu_datas, but the one associated to its virtual id. So we first
	 * need to translate the redistributor address.
	 */
	for_each_cpu(cpu, cell->cpu_set) {
		virt_id = arm_cpu_phys2virt(cpu);
		offs = per_cpu(virt_id)->gicr_base - gicr_base;
		if (mmio->address >= offs &&
		    mmio->address < offs + redist_size) {
			phys_redist = per_cpu(cpu)->gicr_base;
			break;
		}
	}

	if (phys_redist == NULL)
		return MMIO_ERROR;

	mmio->address -= offs;

	/* Change the ID register, all other accesses are allowed. */
	if (!mmio->is_write) {
		switch (mmio->address) {
		case GICR_TYPER:
			if (virt_id == cell->arch.last_virt_id)
				mmio->value = GICR_TYPER_Last;
			else
				mmio->value = 0;
			/* AArch64 can use a writeq for this register */
			if (mmio->size == 8)
				mmio->value |= (u64)virt_id << 32;

			return MMIO_HANDLED;
		case GICR_TYPER + 4:
			/* Upper bits contain the affinity */
			mmio->value = virt_id;
			return MMIO_HANDLED;
		}
	}
	mmio_perform_access(phys_redist, mmio);
	return MMIO_HANDLED;
}

static int gic_cell_init(struct cell *cell)
{
	mmio_region_register(cell, system_config->platform_info.arm.gicr_base,
			     GICR_SIZE, gic_handle_redist_access, NULL);

	return 0;
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

void gicv3_handle_sgir_write(u64 sgir)
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

	/* Translate the virtual cpu id into the physical one */
	if (mmio->is_write) {
		mmio->value = arm_cpu_virt2phys(cell, mmio->value);
		if (mmio->value == -1) {
			printk("Attempt to route IRQ%d outside of cell\n", irq);
			return MMIO_ERROR;
		}
		mmio_perform_access(gicd_base, mmio);
	} else {
		cpu = mmio_read32(gicd_base + GICD_IROUTER + 8 * irq);
		mmio->value = arm_cpu_phys2virt(cpu);
	}
	return MMIO_HANDLED;
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
	return 2;
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
