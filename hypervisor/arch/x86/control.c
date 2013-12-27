/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <asm/vmx.h>
#include <asm/vtd.h>

static void flush_linux_cpu_caches(struct per_cpu *cpu_data)
{
	unsigned int cpu;

	for_each_cpu_except(cpu, linux_cell.cpu_set, cpu_data->cpu_id)
		per_cpu(cpu)->flush_caches = true;
}

int arch_cell_create(struct per_cpu *cpu_data, struct cell *cell)
{
	int err;

	/* TODO: Implement proper roll-backs on errors */

	vmx_linux_cell_shrink(cell->config);
	flush_linux_cpu_caches(cpu_data);
	err = vmx_cell_init(cell);
	if (err)
		return err;

	vtd_linux_cell_shrink(cell->config);
	err = vtd_cell_init(cell);
	if (err)
		vmx_cell_exit(cell);
	return err;
}

int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	return vmx_map_memory_region(cell, mem);
}

void arch_unmap_memory_region(struct cell *cell,
			      const struct jailhouse_memory *mem)
{
	vmx_unmap_memory_region(cell, mem);
}

void arch_cell_destroy(struct per_cpu *cpu_data, struct cell *cell)
{
	vmx_cell_exit(cell);
	flush_linux_cpu_caches(cpu_data);
}
