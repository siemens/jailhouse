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

int arch_cell_create(struct per_cpu *cpu_data, struct cell *new_cell,
		     struct jailhouse_cell_desc *config)
{
	unsigned int cpu;

	vmx_cell_shrink(cpu_data->cell, config);

	for_each_cpu_except(cpu, cpu_data->cell->cpu_set, cpu_data->cpu_id)
		per_cpu(cpu)->flush_caches = true;

	return vmx_cell_init(new_cell, config);
}
