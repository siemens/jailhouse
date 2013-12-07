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

static void flush_linux_cpu_caches(struct per_cpu *cpu_data)
{
	unsigned int cpu;

	for_each_cpu_except(cpu, linux_cell.cpu_set, cpu_data->cpu_id)
		per_cpu(cpu)->flush_caches = true;
}

int arch_cell_create(struct per_cpu *cpu_data, struct cell *cell)
{
	vmx_cell_shrink(&linux_cell, cell->config);
	flush_linux_cpu_caches(cpu_data);
	return vmx_cell_init(cell);
}
