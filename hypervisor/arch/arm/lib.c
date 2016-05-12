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
#include <jailhouse/string.h>
#include <jailhouse/types.h>
#include <asm/sysregs.h>

unsigned long phys_processor_id(void)
{
	unsigned long mpidr;

	arm_read_sysreg(MPIDR_EL1, mpidr);
	return mpidr & MPIDR_CPUID_MASK;
}
