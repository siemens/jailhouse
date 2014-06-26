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

#ifndef _JAILHOUSE_ASM_TRAPS_H
#define _JAILHOUSE_ASM_TRAPS_H

#include <jailhouse/types.h>
#include <asm/head.h>
#include <asm/percpu.h>

#ifndef __ASSEMBLY__

enum trap_return {
	TRAP_HANDLED		= 1,
	TRAP_UNHANDLED		= 0,
};

struct trap_context {
	unsigned long *regs;
	u32 esr;
	u32 cpsr;
};

typedef int (*trap_handler)(struct per_cpu *cpu_data,
			     struct trap_context *ctx);

#define arm_read_banked_reg(reg, val)					\
	asm volatile ("mrs %0, " #reg "\n" : "=r" (val))

#define arm_write_banked_reg(reg, val)					\
	asm volatile ("msr " #reg ", %0\n" : : "r" (val))

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_TRAPS_H */
