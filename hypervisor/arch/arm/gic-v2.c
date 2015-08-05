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
#include <asm/gic_common.h>
#include <asm/irqchip.h>
#include <asm/platform.h>
#include <asm/setup.h>

static unsigned int gic_num_lr;

extern void *gicd_base;
extern unsigned int gicd_size;
void *gicc_base;
unsigned int gicc_size;
void *gicv_base;
void *gich_base;
unsigned int gich_size;

static int gic_init(void)
{
	int err;

	/* FIXME: parse device tree */
	gicc_base = GICC_BASE;
	gicc_size = GICC_SIZE;
	gich_base = GICH_BASE;
	gich_size = GICH_SIZE;
	gicv_base = GICV_BASE;

	err = arch_map_device(gicc_base, gicc_base, gicc_size);
	if (err)
		return err;

	err = arch_map_device(gich_base, gich_base, gich_size);

	return err;
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

static int gic_cpu_reset(struct per_cpu *cpu_data, bool is_shutdown)
{
	unsigned int i;
	bool root_shutdown = is_shutdown && (cpu_data->cell == &root_cell);
	u32 active;
	u32 gich_vmcr = 0;
	u32 gicc_ctlr, gicc_pmr;

	gic_clear_pending_irqs();

	/* Deactivate all PPIs */
	active = mmio_read32(gicd_base + GICD_ISACTIVER);
	for (i = 16; i < 32; i++) {
		if (test_bit(i, (unsigned long *)&active))
			mmio_write32(gicc_base + GICC_DIR, i);
	}

	/* Disable PPIs if necessary */
	if (!root_shutdown)
		mmio_write32(gicd_base + GICD_ICENABLER, 0xffff0000);
	/* Ensure IPIs are enabled */
	mmio_write32(gicd_base + GICD_ISENABLER, 0x0000ffff);

	if (is_shutdown)
		mmio_write32(gich_base + GICH_HCR, 0);

	if (root_shutdown) {
		gich_vmcr = mmio_read32(gich_base + GICH_VMCR);
		gicc_ctlr = 0;
		gicc_pmr = (gich_vmcr >> GICH_VMCR_PMR_SHIFT) << GICV_PMR_SHIFT;

		if (gich_vmcr & GICH_VMCR_EN0)
			gicc_ctlr |= GICC_CTLR_GRPEN1;
		if (gich_vmcr & GICH_VMCR_EOImode)
			gicc_ctlr |= GICC_CTLR_EOImode;

		mmio_write32(gicc_base + GICC_CTLR, gicc_ctlr);
		mmio_write32(gicc_base + GICC_PMR, gicc_pmr);

		gich_vmcr = 0;
	}
	mmio_write32(gich_base + GICH_VMCR, gich_vmcr);

	return 0;
}

static int gic_cpu_init(struct per_cpu *cpu_data)
{
	u32 vtr, vmcr;
	u32 cell_gicc_ctlr, cell_gicc_pmr;

	/* Ensure all IPIs are enabled */
	mmio_write32(gicd_base + GICD_ISENABLER, 0x0000ffff);

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

	/* Register ourselves into the CPU itf map */
	gic_probe_cpu_id(cpu_data->cpu_id);

	return 0;
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
	 * target_cpu_map has not been populated by all available CPUs when the
	 * setup code initialises the root cell. It is assumed that the kernel
	 * already has configured all its SPIs anyway, and that it will redirect
	 * them when unplugging a CPU.
	 */
	if (cell != &root_cell)
		gic_target_spis(cell, cell);

	/*
	 * Let the guest access the virtual CPU interface instead of the
	 * physical one.
	 *
	 * WARN: some SoCs (EXYNOS4) use a modified GIC which doesn't have any
	 * banked CPU interface, so we should map per-CPU physical addresses
	 * here.
	 * As for now, none of them seem to have virtualization extensions.
	 */
	return paging_create(&cell->arch.mm, (unsigned long)gicv_base,
			     gicc_size, (unsigned long)gicc_base,
			     (PTE_FLAG_VALID | PTE_ACCESS_FLAG |
			      S2_PTE_ACCESS_RW | S2_PTE_FLAG_DEVICE),
			     PAGING_NON_COHERENT);
}

static void gic_cell_exit(struct cell *cell)
{
	paging_destroy(&cell->arch.mm, (unsigned long)gicc_base, gicc_size,
		       PAGING_NON_COHERENT);
	/* Reset interrupt routing of the cell's spis */
	gic_target_spis(cell, &root_cell);
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

static int gic_inject_irq(struct per_cpu *cpu_data, struct pending_irq *irq)
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
		if ((lr & GICH_LR_VIRT_ID_MASK) == irq->virt_id)
			return -EINVAL;
	}

	if (first_free == -1) {
		/* Enable maintenance IRQ */
		u32 hcr;
		hcr = mmio_read32(gich_base + GICH_HCR);
		hcr |= GICH_HCR_UIE;
		mmio_write32(gich_base + GICH_HCR, hcr);

		return -EBUSY;
	}

	/* Inject group 0 interrupt (seen as IRQ by the guest) */
	lr = irq->virt_id;
	lr |= GICH_LR_PENDING_BIT;

	if (irq->hw) {
		lr |= GICH_LR_HW_BIT;
		lr |= irq->type.irq << GICH_LR_PHYS_ID_SHIFT;
	} else {
		lr |= irq->type.sgi.cpuid << GICH_LR_CPUID_SHIFT;
		if (irq->type.sgi.maintenance)
			lr |= GICH_LR_SGI_EOI_BIT;
	}

	gic_write_lr(first_free, lr);

	return 0;
}

static int gic_mmio_access(struct per_cpu *cpu_data,
			   struct mmio_access *mmio)
{
	void *address = (void *)mmio->address;

	if (address >= gicd_base && address < gicd_base + gicd_size)
		return gic_handle_dist_access(cpu_data, mmio);

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
