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

#ifndef _JAILHOUSE_ASM_PERCPU_H
#define _JAILHOUSE_ASM_PERCPU_H

#include <jailhouse/types.h>
#include <asm/paging.h>

#define NUM_ENTRY_REGS			13

/* Keep in sync with struct per_cpu! */
#define PERCPU_SIZE_SHIFT		13
#define PERCPU_STACK_END		PAGE_SIZE
#define PERCPU_LINUX_SP			PERCPU_STACK_END

#ifndef __ASSEMBLY__

#include <asm/cell.h>

struct per_cpu {
	/* Keep these two in sync with defines above! */
	u8 stack[PAGE_SIZE];
	unsigned long linux_sp;
	unsigned long linux_ret;
	unsigned long linux_flags;
	unsigned long linux_reg[NUM_ENTRY_REGS];

	struct per_cpu *cpu_data;
	unsigned int cpu_id;
//	u32 apic_id;
	struct cell *cell;

	u32 stats[JAILHOUSE_NUM_CPU_STATS];

	bool initialized;

	bool flush_caches;
	bool shutdown_cpu;
	int shutdown_state;
	bool failed;
} __attribute__((aligned(PAGE_SIZE)));

#define DEFINE_PER_CPU_ACCESSOR(field)					    \
static inline typeof(((struct per_cpu *)0)->field) this_##field(void)	    \
{									    \
	typeof(((struct per_cpu *)0)->field) tmp = 0;			    \
									    \
	return tmp;							    \
}

DEFINE_PER_CPU_ACCESSOR(cpu_data)
DEFINE_PER_CPU_ACCESSOR(cpu_id)
DEFINE_PER_CPU_ACCESSOR(cell)

static inline struct per_cpu *per_cpu(unsigned int cpu)
{
	extern u8 __page_pool[];

	return (struct per_cpu *)(__page_pool + (cpu << PERCPU_SIZE_SHIFT));
}

/* Validate defines */
#define CHECK_ASSUMPTION(assume)	((void)sizeof(char[1 - 2*!(assume)]))

static inline void __check_assumptions(void)
{
	struct per_cpu cpu_data;

	CHECK_ASSUMPTION(sizeof(struct per_cpu) == (1 << PERCPU_SIZE_SHIFT));
	CHECK_ASSUMPTION(sizeof(cpu_data.stack) == PERCPU_STACK_END);
	CHECK_ASSUMPTION(__builtin_offsetof(struct per_cpu, linux_sp) ==
			 PERCPU_LINUX_SP);
}
#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PERCPU_H */
