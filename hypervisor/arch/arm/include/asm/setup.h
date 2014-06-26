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

#include <asm/head.h>
#include <asm/percpu.h>

#ifndef __ASSEMBLY__

static inline void __attribute__((always_inline))
cpu_return_el1(struct per_cpu *cpu_data)
{
	/* Return value */
	cpu_data->linux_reg[0] = 0;

	asm volatile(
	/* Reset the hypervisor stack */
	"mov	sp, %4\n"

	"msr	sp_svc, %0\n"
	"msr	elr_hyp, %1\n"
	"msr	spsr_hyp, %2\n"
	/*
	 * We don't care about clobbering the other registers from now on. Must
	 * be in sync with arch_entry.
	 */
	"ldm	%3, {r0 - r12}\n"
	/* After this, the kernel won't be able to access the hypervisor code */
	"eret\n"
	:
	: "r" (cpu_data->linux_sp + (NUM_ENTRY_REGS * sizeof(unsigned long))),
	  "r" (cpu_data->linux_ret),
	  "r" (cpu_data->linux_flags),
	  "r" (cpu_data->linux_reg),
	  "r" (cpu_data->stack + PERCPU_STACK_END)
	:);
}

int switch_exception_level(struct per_cpu *cpu_data);
inline int arch_map_device(void *paddr, void *vaddr, unsigned long size);
inline int arch_unmap_device(void *addr, unsigned long size);

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_SETUP_H */
