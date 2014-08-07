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

#include <jailhouse/processor.h>
#include <asm/control.h>
#include <asm/irqchip.h>
#include <asm/paging.h>
#include <asm/platform.h>
#include <asm/smp.h>

static unsigned long hotplug_mbox;

static int smp_init(struct cell *cell)
{
	/* vexpress SYSFLAGS */
	hotplug_mbox = SYSREGS_BASE + 0x30;

	/* Map the mailbox page */
	arch_generic_smp_init(hotplug_mbox);

	return 0;
}

static unsigned long smp_spin(struct per_cpu *cpu_data)
{
	return arch_generic_smp_spin(hotplug_mbox);
}

static int smp_mmio(struct per_cpu *cpu_data, struct mmio_access *access)
{
	return arch_generic_smp_mmio(cpu_data, access, hotplug_mbox);
}

static struct smp_ops vexpress_smp_ops = {
	.type = SMP_SPIN,
	.init = smp_init,
	.mmio_handler = smp_mmio,
	.cpu_spin = smp_spin,
};

/*
 * Store the guest's secondaries into our PSCI, and wake them up when we catch
 * an access to the mbox from the primary.
 */
static struct smp_ops vexpress_guest_smp_ops = {
	.type = SMP_SPIN,
	.init = psci_cell_init,
	.mmio_handler = smp_mmio,
	.cpu_spin = psci_emulate_spin,
};

void register_smp_ops(struct cell *cell)
{
	/*
	 * mach-vexpress only writes the SYS_FLAGS once at boot, so the root
	 * cell cannot rely on this write to guess where the secondary CPUs
	 * should return.
	 */
	if (cell == &root_cell)
		cell->arch.smp = &vexpress_smp_ops;
	else
		cell->arch.smp = &vexpress_guest_smp_ops;
}
