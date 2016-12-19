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
#include <jailhouse/printk.h>
#include <asm/gic.h>
#include <asm/irqchip.h>
#include <asm/setup.h>

static unsigned int gic_num_lr;

void *gicc_base;
void *gich_base;

/* Check that the targeted interface belongs to the cell */
static bool gic_targets_in_cell(struct cell *cell, u8 targets)
{
	unsigned int cpu;

	for (cpu = 0; cpu < ARRAY_SIZE(gicv2_target_cpu_map); cpu++)
		if (targets & gicv2_target_cpu_map[cpu] &&
		    per_cpu(cpu)->cell != cell)
			return false;

	return true;
}

static int gic_init(void)
{
	gicc_base = paging_map_device(
			system_config->platform_info.arm.gicc_base, GICC_SIZE);
	if (!gicc_base)
		return -ENOMEM;

	gich_base = paging_map_device(
			system_config->platform_info.arm.gich_base, GICH_SIZE);
	if (!gich_base)
		return -ENOMEM;

	return 0;
}

static void gic_clear_pending_irqs(void)
{
	unsigned int n;

	/* Clear list registers. */
	for (n = 0; n < gic_num_lr; n++)
		gic_write_lr(n, 0);

	/* Clear active priority bits. */
	mmio_write32(gich_base + GICH_APR, 0);
}

static void gic_cpu_reset(struct per_cpu *cpu_data)
{
	unsigned int mnt_irq = system_config->platform_info.arm.maintenance_irq;

	gic_clear_pending_irqs();

	/* Ensure all IPIs and the maintenance PPI are enabled */
	mmio_write32(gicd_base + GICD_ISENABLER, 0x0000ffff | (1 << mnt_irq));

	/* Disable PPIs, except for the maintenance interrupt. */
	mmio_write32(gicd_base + GICD_ICENABLER, 0xffff0000 & ~(1 << mnt_irq));

	/* Deactivate all active PPIs */
	mmio_write32(gicd_base + GICD_ICACTIVER, 0xffff0000);

	mmio_write32(gich_base + GICH_VMCR, 0);
}

static int gic_cpu_init(struct per_cpu *cpu_data)
{
	unsigned int mnt_irq = system_config->platform_info.arm.maintenance_irq;
	u32 vtr, vmcr;
	u32 cell_gicc_ctlr, cell_gicc_pmr;

	/* Ensure all IPIs and the maintenance PPI are enabled. */
	mmio_write32(gicd_base + GICD_ISENABLER, 0x0000ffff | (1 << mnt_irq));

	cell_gicc_ctlr = mmio_read32(gicc_base + GICC_CTLR);
	cell_gicc_pmr = mmio_read32(gicc_base + GICC_PMR);

	mmio_write32(gicc_base + GICC_CTLR,
		     GICC_CTLR_GRPEN1 | GICC_CTLR_EOImode);
	mmio_write32(gicc_base + GICC_PMR, GICC_PMR_DEFAULT);

	vtr = mmio_read32(gich_base + GICH_VTR);
	gic_num_lr = (vtr & 0x3f) + 1;

	/* VMCR only contains 5 bits of priority */
	vmcr = (cell_gicc_pmr >> GICV_PMR_SHIFT) << GICH_VMCR_PMR_SHIFT;
	/*
	 * All virtual interrupts are group 0 in this driver since the GICV
	 * layout seen by the guest corresponds to GICC without security
	 * extensions:
	 * - A read from GICV_IAR doesn't acknowledge group 1 interrupts
	 *   (GICV_AIAR does it, but the guest never attempts to accesses it)
	 * - A write to GICV_CTLR.GRP0EN corresponds to the GICC_CTLR.GRP1EN bit
	 *   Since the guest's driver thinks that it is accessing a GIC with
	 *   security extensions, a write to GPR1EN will enable group 0
	 *   interrups.
	 * - Group 0 interrupts are presented as virtual IRQs (FIQEn = 0)
	 */
	if (cell_gicc_ctlr & GICC_CTLR_GRPEN1)
		vmcr |= GICH_VMCR_EN0;
	if (cell_gicc_ctlr & GICC_CTLR_EOImode)
		vmcr |= GICH_VMCR_EOImode;

	mmio_write32(gich_base + GICH_VMCR, vmcr);
	mmio_write32(gich_base + GICH_HCR, GICH_HCR_EN);

	/*
	 * Clear pending virtual IRQs in case anything is left from previous
	 * use. Physically pending IRQs will be forwarded to Linux once we
	 * enable interrupts for the hypervisor.
	 */
	gic_clear_pending_irqs();

	cpu_data->gicc_initialized = true;

	/*
	 * Get the CPU interface ID for this cpu. It can be discovered by
	 * reading the banked value of the PPI and IPI TARGET registers
	 * Patch 2bb3135 in Linux explains why the probe may need to scans the
	 * first 8 registers: some early implementation returned 0 for the first
	 * ITARGETSR registers.
	 * Since those didn't have virtualization extensions, we can safely
	 * ignore that case.
	 */
	if (cpu_data->cpu_id >= ARRAY_SIZE(gicv2_target_cpu_map))
		return -EINVAL;

	gicv2_target_cpu_map[cpu_data->cpu_id] =
		mmio_read32(gicd_base + GICD_ITARGETSR);

	if (gicv2_target_cpu_map[cpu_data->cpu_id] == 0)
		return -ENODEV;

	return 0;
}

static void gic_cpu_shutdown(struct per_cpu *cpu_data)
{
	u32 gich_vmcr = mmio_read32(gich_base + GICH_VMCR);
	u32 gicc_ctlr = 0;

	if (!cpu_data->gicc_initialized)
		return;

	mmio_write32(gich_base + GICH_HCR, 0);

	/* Disable the maintenance interrupt - not used by Linux. */
	mmio_write32(gicd_base + GICD_ICENABLER,
		     1 << system_config->platform_info.arm.maintenance_irq);

	if (gich_vmcr & GICH_VMCR_EN0)
		gicc_ctlr |= GICC_CTLR_GRPEN1;
	if (gich_vmcr & GICH_VMCR_EOImode)
		gicc_ctlr |= GICC_CTLR_EOImode;

	mmio_write32(gicc_base + GICC_CTLR, gicc_ctlr);
	mmio_write32(gicc_base + GICC_PMR,
		     (gich_vmcr >> GICH_VMCR_PMR_SHIFT) << GICV_PMR_SHIFT);
}

static void gic_eoi_irq(u32 irq_id, bool deactivate)
{
	/*
	 * The GIC doesn't seem to care about the CPUID value written to EOIR,
	 * which is rather convenient...
	 */
	mmio_write32(gicc_base + GICC_EOIR, irq_id);
	if (deactivate)
		mmio_write32(gicc_base + GICC_DIR, irq_id);
}

static int gic_cell_init(struct cell *cell)
{
	/*
	 * Let the guest access the virtual CPU interface instead of the
	 * physical one.
	 *
	 * WARN: some SoCs (EXYNOS4) use a modified GIC which doesn't have any
	 * banked CPU interface, so we should map per-CPU physical addresses
	 * here.
	 * As for now, none of them seem to have virtualization extensions.
	 */
	return paging_create(&cell->arch.mm,
			     system_config->platform_info.arm.gicv_base,
			     GICC_SIZE,
			     system_config->platform_info.arm.gicc_base,
			     (PTE_FLAG_VALID | PTE_ACCESS_FLAG |
			      S2_PTE_ACCESS_RW | S2_PTE_FLAG_DEVICE),
			     PAGING_COHERENT);
}

static void gic_cell_exit(struct cell *cell)
{
	paging_destroy(&cell->arch.mm,
		       system_config->platform_info.arm.gicc_base, GICC_SIZE,
		       PAGING_COHERENT);
}

static void gic_adjust_irq_target(struct cell *cell, u16 irq_id)
{
	void *itargetsr = gicd_base + GICD_ITARGETSR + (irq_id & ~0x3);
	u32 targets = mmio_read32(itargetsr);
	unsigned int shift = (irq_id % 4) * 8;

	if (gic_targets_in_cell(cell, (u8)(targets >> shift)))
		return;

	targets &= ~(0xff << shift);
	targets |= gicv2_target_cpu_map[first_cpu(cell->cpu_set)] << shift;

	mmio_write32(itargetsr, targets);
}

static int gic_send_sgi(struct sgi *sgi)
{
	u32 val;

	if (!is_sgi(sgi->id))
		return -EINVAL;

	val = (sgi->routing_mode & 0x3) << 24
		| (sgi->targets & 0xff) << 16
		| (sgi->id & 0xf);

	mmio_write32(gicd_base + GICD_SGIR, val);

	return 0;
}

static int gic_inject_irq(struct per_cpu *cpu_data, u16 irq_id)
{
	int i;
	int first_free = -1;
	u32 lr;
	unsigned long elsr[2];

	elsr[0] = mmio_read32(gich_base + GICH_ELSR0);
	elsr[1] = mmio_read32(gich_base + GICH_ELSR1);
	for (i = 0; i < gic_num_lr; i++) {
		if (test_bit(i, elsr)) {
			/* Entry is available */
			if (first_free == -1)
				first_free = i;
			continue;
		}

		/* Check that there is no overlapping */
		lr = gic_read_lr(i);
		if ((lr & GICH_LR_VIRT_ID_MASK) == irq_id)
			return -EEXIST;
	}

	if (first_free == -1)
		return -EBUSY;

	/* Inject group 0 interrupt (seen as IRQ by the guest) */
	lr = irq_id;
	lr |= GICH_LR_PENDING_BIT;

	if (!is_sgi(irq_id)) {
		lr |= GICH_LR_HW_BIT;
		lr |= (u32)irq_id << GICH_LR_PHYS_ID_SHIFT;
	}

	gic_write_lr(first_free, lr);

	return 0;
}

static void gic_enable_maint_irq(bool enable)
{
	u32 hcr;

	hcr = mmio_read32(gich_base + GICH_HCR);
	if (enable)
		hcr |= GICH_HCR_UIE;
	else
		hcr &= ~GICH_HCR_UIE;
	mmio_write32(gich_base + GICH_HCR, hcr);
}

static bool gic_has_pending_irqs(void)
{
	unsigned int n;

	for (n = 0; n < gic_num_lr; n++)
		if (gic_read_lr(n) & GICH_LR_PENDING_BIT)
			return true;

	return false;
}

enum mmio_result gic_handle_irq_route(struct mmio_access *mmio,
				      unsigned int irq)
{
	/* doesn't exist in v2 - ignore access */
	return MMIO_HANDLED;
}

/*
 * GICv2 uses 8bit values for each IRQ in the ITARGETSR registers
 */
static enum mmio_result gic_handle_irq_target(struct mmio_access *mmio,
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

unsigned int irqchip_mmio_count_regions(struct cell *cell)
{
	return 1;
}

struct irqchip_ops irqchip = {
	.init = gic_init,
	.cpu_init = gic_cpu_init,
	.cpu_reset = gic_cpu_reset,
	.cpu_shutdown = gic_cpu_shutdown,
	.cell_init = gic_cell_init,
	.cell_exit = gic_cell_exit,
	.adjust_irq_target = gic_adjust_irq_target,

	.send_sgi = gic_send_sgi,
	.inject_irq = gic_inject_irq,
	.enable_maint_irq = gic_enable_maint_irq,
	.has_pending_irqs = gic_has_pending_irqs,
	.eoi_irq = gic_eoi_irq,

	.handle_irq_target = gic_handle_irq_target,
};
