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
#include <asm/gic_v2.h>
#include <asm/irqchip.h>

/* The GICv2 interface numbering does not necessarily match the logical map */
static u8 gicv2_target_cpu_map[8];
static unsigned int gic_num_lr;

static void *gicc_base;
static void *gich_base;

static u32 gicv2_read_lr(unsigned int i)
{
	return mmio_read32(gich_base + GICH_LR_BASE + i * 4);
}

static void gicv2_write_lr(unsigned int i, u32 value)
{
	mmio_write32(gich_base + GICH_LR_BASE + i * 4, value);
}

/* Check that the targeted interface belongs to the cell */
static bool gicv2_targets_in_cell(struct cell *cell, u8 targets)
{
	unsigned int cpu;

	for (cpu = 0; cpu < ARRAY_SIZE(gicv2_target_cpu_map); cpu++)
		if (targets & gicv2_target_cpu_map[cpu] &&
		    per_cpu(cpu)->cell != cell)
			return false;

	return true;
}

static int gicv2_init(void)
{
	/* Probe the GICD version */
	if (GICD_PIDR2_ARCH(mmio_read32(gicd_base + GICDv2_PIDR2)) != 2)
		return trace_error(-ENODEV);

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

static void gicv2_clear_pending_irqs(void)
{
	unsigned int n;

	/* Clear list registers. */
	for (n = 0; n < gic_num_lr; n++)
		gicv2_write_lr(n, 0);

	/* Clear active priority bits. */
	mmio_write32(gich_base + GICH_APR, 0);
}

static void gicv2_cpu_reset(struct per_cpu *cpu_data)
{
	unsigned int mnt_irq = system_config->platform_info.arm.maintenance_irq;

	gicv2_clear_pending_irqs();

	/* Ensure all IPIs and the maintenance PPI are enabled */
	mmio_write32(gicd_base + GICD_ISENABLER, 0x0000ffff | (1 << mnt_irq));

	/* Disable PPIs, except for the maintenance interrupt. */
	mmio_write32(gicd_base + GICD_ICENABLER, 0xffff0000 & ~(1 << mnt_irq));

	/* Deactivate all active PPIs */
	mmio_write32(gicd_base + GICD_ICACTIVER, 0xffff0000);

	mmio_write32(gich_base + GICH_VMCR, 0);
}

static int gicv2_cpu_init(struct per_cpu *cpu_data)
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
	gicv2_clear_pending_irqs();

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

static int gicv2_cpu_shutdown(struct per_cpu *cpu_data)
{
	u32 gich_vmcr = mmio_read32(gich_base + GICH_VMCR);
	u32 gicc_ctlr = 0;

	if (!cpu_data->gicc_initialized)
		return -ENODEV;

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

	return 0;
}

static u32 gicv2_read_iar_irqn(void)
{
	return mmio_read32(gicc_base + GICC_IAR) & 0x3ff;
}

static void gicv2_eoi_irq(u32 irq_id, bool deactivate)
{
	/*
	 * The GIC doesn't seem to care about the CPUID value written to EOIR,
	 * which is rather convenient...
	 */
	mmio_write32(gicc_base + GICC_EOIR, irq_id);
	if (deactivate)
		mmio_write32(gicc_base + GICC_DIR, irq_id);
}

static int gicv2_cell_init(struct cell *cell)
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

static void gicv2_cell_exit(struct cell *cell)
{
	paging_destroy(&cell->arch.mm,
		       system_config->platform_info.arm.gicc_base, GICC_SIZE,
		       PAGING_COHERENT);
}

static void gicv2_adjust_irq_target(struct cell *cell, u16 irq_id)
{
	void *itargetsr = gicd_base + GICD_ITARGETSR + (irq_id & ~0x3);
	u32 targets = mmio_read32(itargetsr);
	unsigned int shift = (irq_id % 4) * 8;

	if (gicv2_targets_in_cell(cell, (u8)(targets >> shift)))
		return;

	targets &= ~(0xff << shift);
	targets |= gicv2_target_cpu_map[first_cpu(cell->cpu_set)] << shift;

	mmio_write32(itargetsr, targets);
}

static int gicv2_send_sgi(struct sgi *sgi)
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

static int gicv2_inject_irq(u16 irq_id, u16 sender)
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
		lr = gicv2_read_lr(i);
		if ((lr & GICH_LR_VIRT_ID_MASK) == irq_id)
			return -EEXIST;
	}

	if (first_free == -1)
		return -EBUSY;

	/* Inject group 0 interrupt (seen as IRQ by the guest) */
	lr = irq_id;
	lr |= GICH_LR_PENDING_BIT;

	if (is_sgi(irq_id)) {
		lr |= (sender & 0x7) << GICH_LR_CPUID_SHIFT;
	} else {
		lr |= GICH_LR_HW_BIT;
		lr |= (u32)irq_id << GICH_LR_PHYS_ID_SHIFT;
	}

	gicv2_write_lr(first_free, lr);

	return 0;
}

static void gicv2_enable_maint_irq(bool enable)
{
	u32 hcr;

	hcr = mmio_read32(gich_base + GICH_HCR);
	if (enable)
		hcr |= GICH_HCR_UIE;
	else
		hcr &= ~GICH_HCR_UIE;
	mmio_write32(gich_base + GICH_HCR, hcr);
}

static bool gicv2_has_pending_irqs(void)
{
	unsigned int n;

	for (n = 0; n < gic_num_lr; n++)
		if (gicv2_read_lr(n) & GICH_LR_PENDING_BIT)
			return true;

	return false;
}

static int gicv2_get_pending_irq(void)
{
	unsigned int n;
	u64 lr;

	for (n = 0; n < gic_num_lr; n++) {
		lr = gicv2_read_lr(n);
		if (lr & GICH_LR_PENDING_BIT) {
			gicv2_write_lr(n, 0);
			return lr & GICH_LR_VIRT_ID_MASK;
		}
	}

	return -ENOENT;
}

static void gicv2_inject_phys_irq(u16 irq_id)
{
	unsigned int offset = (irq_id / 32) * 4;
	unsigned int mask = 1 << (irq_id % 32);

	if (is_sgi(irq_id)) {
		/* Inject with CPU 0 as source - we don't track the origin. */
		mmio_write8(gicd_base + GICD_SPENDSGIR + irq_id, 1);
	} else {
		/*
		 * Hardware interrupts are physically active until they are
		 * processed by the cell. Deactivate them first so that we can
		 * reinject.
		 */
		mmio_write32(gicd_base + GICD_ICACTIVER + offset, mask);

		/* inject via GICD */
		mmio_write32(gicd_base + GICD_ISPENDR + offset, mask);
	}
}

static enum mmio_result gicv2_handle_irq_route(struct mmio_access *mmio,
					       unsigned int irq)
{
	/* doesn't exist in v2 - ignore access */
	return MMIO_HANDLED;
}

/*
 * GICv2 uses 8bit values for each IRQ in the ITARGETSR registers
 */
static enum mmio_result gicv2_handle_irq_target(struct mmio_access *mmio,
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

		if (!gicv2_targets_in_cell(cell, targets)) {
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

static enum mmio_result gicv2_handle_dist_access(struct mmio_access *mmio)
{
	unsigned long val = mmio->value;
	struct sgi sgi;

	switch (mmio->address) {
	case GICD_SGIR:
		if (!mmio->is_write)
			return MMIO_HANDLED;

		sgi.targets = (val >> 16) & 0xff;
		sgi.routing_mode = (val >> 24) & 0x3;
		sgi.cluster_id = 0;
		sgi.id = val & 0xf;

		gic_handle_sgir_write(&sgi);
		return MMIO_HANDLED;

	case GICD_CTLR:
	case GICD_TYPER:
	case GICD_IIDR:
	case REG_RANGE(GICDv2_PIDR0, 4, 4):
	case REG_RANGE(GICDv2_PIDR4, 4, 4):
	case REG_RANGE(GICDv2_CIDR0, 4, 4):
		/* Allow read access, ignore write */
		if (!mmio->is_write)
			mmio_perform_access(gicd_base, mmio);
		/* fall through */
	default:
		/* Ignore access. */
		return MMIO_HANDLED;
	}
}

static int gicv2_get_cpu_target(unsigned int cpu_id)
{
	return gicv2_target_cpu_map[cpu_id];
}

static u64 gicv2_get_cluster_target(unsigned int cpu_id)
{
	return 0;
}

const struct irqchip gicv2_irqchip = {
	.init = gicv2_init,
	.cpu_init = gicv2_cpu_init,
	.cpu_reset = gicv2_cpu_reset,
	.cpu_shutdown = gicv2_cpu_shutdown,
	.cell_init = gicv2_cell_init,
	.cell_exit = gicv2_cell_exit,
	.adjust_irq_target = gicv2_adjust_irq_target,

	.send_sgi = gicv2_send_sgi,
	.inject_irq = gicv2_inject_irq,
	.enable_maint_irq = gicv2_enable_maint_irq,
	.has_pending_irqs = gicv2_has_pending_irqs,
	.read_iar_irqn = gicv2_read_iar_irqn,
	.eoi_irq = gicv2_eoi_irq,

	.get_pending_irq = gicv2_get_pending_irq,
	.inject_phys_irq = gicv2_inject_phys_irq,

	.handle_irq_route = gicv2_handle_irq_route,
	.handle_irq_target = gicv2_handle_irq_target,
	.handle_dist_access = gicv2_handle_dist_access,
	.get_cpu_target = gicv2_get_cpu_target,
	.get_cluster_target = gicv2_get_cluster_target,

	.gicd_size = 0x1000,
};
