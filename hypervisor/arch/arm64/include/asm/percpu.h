/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/cell.h>
#include <asm/irqchip.h>

struct per_cpu {
	u8 stack[PAGE_SIZE];

	/** Per-CPU root page table. */
	u8 root_table_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
	/** Per-CPU paging structures. */
	struct paging_structures pg_structs;

	/* common fields */
	unsigned int cpu_id;
	struct cell *cell;
	u32 stats[JAILHOUSE_NUM_CPU_STATS];
	int shutdown_state;
	bool failed;
	unsigned long id_aa64mmfr0;

	struct pending_irqs pending_irqs;

	union {
		/** Only GICv2: per-cpu initialization completed. */
		bool gicc_initialized;
		/** Only GICv3 */
		struct {
			/** mapped redistributor base. When non-NULL, per-cpu
			 * cpu initialization completed.*/
			void *base;
			/** physical redistributor address */
			unsigned long phys_addr;
		} gicr;
	};

	unsigned long mpidr;

	/**
	 * Lock protecting CPU state changes done for control tasks.
	 *
	 * The lock protects the following fields (unless CPU is suspended):
	 * @li per_cpu::suspend_cpu
	 * @li per_cpu::cpu_suspended (except for spinning on it to become
	 *                             true)
	 * @li per_cpu::flush_vcpu_caches
	 */
	spinlock_t control_lock;

	/** Set to true for instructing the CPU to suspend. */
	volatile bool suspend_cpu;
	/** True if CPU is waiting for power-on. */
	volatile bool wait_for_poweron;
	/** True if CPU is suspended. */
	volatile bool cpu_suspended;
	/** Set to true for pending reset. */
	bool reset;
	/** Set to true for pending park. */
	bool park;
	/** Set to true for a pending TLB flush for the paging layer that does
	 *  host physical <-> guest physical memory mappings. */
	bool flush_vcpu_caches;

	unsigned long cpu_on_entry;
	unsigned long cpu_on_context;
} __attribute__((aligned(PAGE_SIZE)));

static inline struct per_cpu *this_cpu_data(void)
{
	return (struct per_cpu *)LOCAL_CPU_BASE;
}

static inline unsigned int this_cpu_id(void)
{
	return this_cpu_data()->cpu_id;
}

static inline struct cell *this_cell(void)
{
	return this_cpu_data()->cell;
}

static inline struct per_cpu *per_cpu(unsigned int cpu)
{
	extern u8 __page_pool[];

	return (struct per_cpu *)(__page_pool + cpu * sizeof(struct per_cpu));
}

static inline struct registers *guest_regs(struct per_cpu *cpu_data)
{
	/* assumes that the cell registers are at the beginning of the stack */
	return (struct registers *)(cpu_data->stack + sizeof(cpu_data->stack)
			- sizeof(struct registers));
}
