/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <asm/control.h>
#include <asm/platform.h>
#include <asm/setup.h>
#include <asm/smp.h>

#define VEXPRESS_FLAGSSET	0x30

const unsigned int smp_mmio_regions = 1;

static unsigned long root_entry;

static enum mmio_result smp_mmio(void *arg, struct mmio_access *mmio)
{
	struct per_cpu *target_data, *cpu_data = this_cpu_data();
	unsigned int cpu;

	if (mmio->address != VEXPRESS_FLAGSSET || !mmio->is_write)
		/* Ignore all other accesses */
		return MMIO_HANDLED;

	for_each_cpu_except(cpu, cpu_data->cell->cpu_set, cpu_data->cpu_id) {
		target_data = per_cpu(cpu);

		arch_suspend_cpu(cpu);

		spin_lock(&target_data->control_lock);

		if (target_data->wait_for_poweron) {
			target_data->cpu_on_entry = mmio->value;
			target_data->cpu_on_context = 0;
			target_data->reset = true;
		}

		spin_unlock(&target_data->control_lock);

		arch_resume_cpu(cpu);
	}

	return MMIO_HANDLED;
}

int smp_init(void)
{
	int err;

	err = arch_map_device(SYSREGS_BASE, SYSREGS_BASE, PAGE_SIZE);
	if (err)
		return err;
	root_entry = mmio_read32(SYSREGS_BASE + VEXPRESS_FLAGSSET);
	arch_unmap_device(SYSREGS_BASE, PAGE_SIZE);

	smp_cell_init(&root_cell);

	return 0;
}

void smp_cell_init(struct cell *cell)
{
	mmio_region_register(cell, (unsigned long)SYSREGS_BASE, PAGE_SIZE,
			     smp_mmio, NULL);
}

void smp_cell_exit(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu(cpu, cell->cpu_set) {
		per_cpu(cpu)->cpu_on_entry = root_entry;
		per_cpu(cpu)->cpu_on_context = 0;
		arch_suspend_cpu(cpu);
		arch_reset_cpu(cpu);
	}
}
