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
#include <jailhouse/processor.h>
#include <asm/irqchip.h>
#include <asm/paging.h>
#include <asm/platform.h>
#include <asm/setup.h>
#include <asm/smp.h>

const unsigned int smp_mmio_regions = 1;

static const unsigned long hotplug_mbox = SYSREGS_BASE + 0x30;

static enum mmio_result smp_mmio(void *arg, struct mmio_access *mmio)
{
	struct per_cpu *cpu_data = this_cpu_data();
	unsigned int cpu;

	if (mmio->address != (hotplug_mbox & PAGE_OFFS_MASK) || !mmio->is_write)
		/* Ignore all other accesses */
		return MMIO_HANDLED;

	for_each_cpu_except(cpu, cpu_data->cell->cpu_set, cpu_data->cpu_id) {
		per_cpu(cpu)->guest_mbox.entry = mmio->value;
		psci_try_resume(cpu);
	}

	return MMIO_HANDLED;
}

static int smp_init(struct cell *cell)
{
	void *mbox_page = (void *)(hotplug_mbox & PAGE_MASK);
	int err;

	/* Map the mailbox page */
	err = arch_map_device(mbox_page, mbox_page, PAGE_SIZE);
	if (err) {
		printk("Unable to map spin mbox page\n");
		return err;
	}

	mmio_region_register(cell, (unsigned long)mbox_page, PAGE_SIZE,
			     smp_mmio, NULL);
	return 0;
}

static unsigned long smp_spin(struct per_cpu *cpu_data)
{
	/*
	 * This is super-dodgy: we assume nothing wrote to the flag register
	 * since the kernel called smp_prepare_cpus, at initialisation.
	 */
	return mmio_read32((void *)hotplug_mbox);
}

static struct smp_ops vexpress_smp_ops = {
	.init = smp_init,
	.cpu_spin = smp_spin,
};

/*
 * Store the guest's secondaries into our PSCI, and wake them up when we catch
 * an access to the mbox from the primary.
 */
static struct smp_ops vexpress_guest_smp_ops = {
	.init = psci_cell_init,
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
