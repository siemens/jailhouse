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

#include <asm/percpu.h>
#include <asm/smp.h>

const unsigned int __attribute__((weak)) smp_mmio_regions;

unsigned long arch_smp_spin(struct per_cpu *cpu_data, struct smp_ops *ops)
{
	/*
	 * Hotplugging CPU0 is not currently supported. It is always assumed to
	 * be the primary CPU. This is consistent with the linux behavior on
	 * most platforms.
	 * The guest image always starts at virtual address 0.
	 */
	if (cpu_data->virt_id == 0)
		return 0;

	return ops->cpu_spin(cpu_data);
}

int __attribute__((weak)) smp_init(void)
{
	return psci_cell_init(&root_cell);
}

void __attribute__((weak)) smp_cell_init(struct cell *cell)
{
}

void __attribute__((weak)) smp_cell_exit(struct cell *cell)
{
}
