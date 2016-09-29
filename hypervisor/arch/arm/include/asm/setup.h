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

#ifndef _JAILHOUSE_ASM_SETUP_H
#define _JAILHOUSE_ASM_SETUP_H

#include <asm/percpu.h>

#ifndef __ASSEMBLY__

static inline void __attribute__((always_inline))
cpu_prepare_return_el1(struct per_cpu *cpu_data, int return_code)
{
	cpu_data->linux_reg[0] = return_code;

	asm volatile (
		"msr	sp_svc, %0\n\t"
		"msr	elr_hyp, %1\n\t"
		"msr	spsr_hyp, %2\n\t"
		:
		: "r" (cpu_data->linux_sp +
		       (NUM_ENTRY_REGS * sizeof(unsigned long))),
		  "r" (cpu_data->linux_ret),
		  "r" (cpu_data->linux_flags));
}

int switch_exception_level(struct per_cpu *cpu_data);

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_SETUP_H */
