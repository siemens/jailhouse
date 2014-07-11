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

#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <jailhouse/types.h>
#include <asm/gic_common.h>
#include <asm/irqchip.h>
#include <asm/platform.h>
#include <asm/setup.h>

/*
 * This implementation assumes that the kernel driver already initialised most
 * of the GIC.
 * There is almost no instruction barrier, since IRQs are always disabled in the
 * hyp, and ERET serves as the context synchronization event.
 */

static unsigned int gic_num_lr;

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

static int gic_cpu_init(struct per_cpu *cpu_data)
{
	u64 typer;
	u32 pidr;
	u32 gic_version;
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

static void gic_handle_irq(struct per_cpu *cpu_data)
{
}

struct irqchip_ops gic_irqchip = {
	.init = gic_init,
	.cpu_init = gic_cpu_init,
	.send_sgi = gic_send_sgi,
	.handle_irq = gic_handle_irq,
};
