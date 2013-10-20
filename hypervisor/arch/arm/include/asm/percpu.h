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

#include <asm/types.h>
#include <asm/paging.h>

#define NUM_ENTRY_REGS			6

/* Keep in sync with struct per_cpu! */
#define PERCPU_SIZE_SHIFT		13
#define PERCPU_STACK_END		PAGE_SIZE
#define PERCPU_LINUX_SP			PERCPU_STACK_END
#define PERCPU_CPU_ID			(PERCPU_LINUX_SP + 4)

#ifndef __ASSEMBLY__

#include <asm/cell.h>

struct per_cpu {
	/* Keep these three in sync with defines above! */
	u8 stack[PAGE_SIZE];
	unsigned long linux_sp;
	unsigned int cpu_id;

//	u32 apic_id;
	struct cell *cell;

	unsigned long linux_reg[NUM_ENTRY_REGS];
//	unsigned long linux_ip;
	bool initialized;

	volatile bool stop_cpu;
	volatile bool wait_for_sipi;
	volatile bool cpu_stopped;
	bool init_signaled;
	int sipi_vector;
	bool flush_caches;
	bool shutdown_cpu;
} __attribute__((aligned(PAGE_SIZE)));

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
	CHECK_ASSUMPTION(__builtin_offsetof(struct per_cpu, cpu_id) ==
			 PERCPU_CPU_ID);
}
#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PERCPU_H */
