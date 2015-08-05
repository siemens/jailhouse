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

#include <jailhouse/mmio.h>
#include <asm/smp.h>
#include <asm/traps.h>

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

int arch_smp_mmio_access(struct mmio_access *access)
{
	struct smp_ops *smp_ops = this_cell()->arch.smp;

	if (smp_ops->mmio_handler)
		return smp_ops->mmio_handler(access);

	return TRAP_UNHANDLED;
}
