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
#include <asm/control.h>
#include <asm/sysregs.h>

unsigned long phys_processor_id(void)
{
	unsigned long mpidr;

	arm_read_sysreg(MPIDR_EL1, mpidr);
	return mpidr & MPIDR_CPUID_MASK;
}

unsigned int cpu_by_phys_processor_id(u64 phys_id)
{
	unsigned int cpu;

	for (cpu = 0; cpu < system_config->root_cell.num_cpus; cpu++)
		if (phys_id == (public_per_cpu(cpu)->mpidr & MPIDR_CPUID_MASK))
			return cpu;

	return INVALID_CPU_ID;
}
