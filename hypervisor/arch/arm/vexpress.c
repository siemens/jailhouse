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
#include <jailhouse/unit.h>
#include <asm/control.h>

#define SYSREGS_BASE		0x1c010000

#define VEXPRESS_FLAGSSET	0x30

static unsigned long root_entry;

static unsigned int vexpress_mmio_count_regions(struct cell *cell)
{
	return 1;
}

static enum mmio_result
sysregs_access_handler(void *arg, struct mmio_access *mmio)
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

static int vexpress_cell_init(struct cell *cell)
{
	mmio_region_register(cell, (unsigned long)SYSREGS_BASE, PAGE_SIZE,
			     sysregs_access_handler, NULL);
	return 0;
}

static void vexpress_cell_exit(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu(cpu, cell->cpu_set) {
		per_cpu(cpu)->cpu_on_entry = root_entry;
		per_cpu(cpu)->cpu_on_context = 0;
		arch_suspend_cpu(cpu);
		arch_reset_cpu(cpu);
	}
}

static int vexpress_init(void)
{
	void *sysregs_base;

	sysregs_base = paging_map_device(SYSREGS_BASE, PAGE_SIZE);
	if (!sysregs_base)
		return -ENOMEM;
	root_entry = mmio_read32(sysregs_base + VEXPRESS_FLAGSSET);
	paging_unmap_device(SYSREGS_BASE, sysregs_base, PAGE_SIZE);

	vexpress_cell_init(&root_cell);

	return 0;
}

DEFINE_UNIT_SHUTDOWN_STUB(vexpress);
DEFINE_UNIT(vexpress, "VExpress");
