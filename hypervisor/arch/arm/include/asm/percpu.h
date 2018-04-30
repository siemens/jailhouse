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

#include <jailhouse/cell.h>
#include <asm/irqchip.h>

#define NUM_ENTRY_REGS			13

struct per_cpu {
	/** Stack used while in hypervisor mode. */
	u8 stack[PAGE_SIZE];

	/** Per-CPU root page table. */
	u8 root_table_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
	/** Per-CPU paging structures. */
	struct paging_structures pg_structs;

	/** Linux stack pointer, used for handover to the hypervisor. */
	unsigned long linux_sp;
	unsigned long linux_ret;
	unsigned long linux_flags;
	unsigned long linux_reg[NUM_ENTRY_REGS];

	unsigned int cpu_id;

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

	struct cell *cell;

	u32 stats[JAILHOUSE_NUM_CPU_STATS];

	bool initialized;

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

	int shutdown_state;
	unsigned long mpidr;
	bool failed;
} __attribute__((aligned(PAGE_SIZE)));

static inline struct registers *guest_regs(struct per_cpu *cpu_data)
{
	/* Assumes that the trap handler is entered with an empty stack */
	return (struct registers *)(cpu_data->stack + sizeof(cpu_data->stack)
			- sizeof(struct registers));
}
