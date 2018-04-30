/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_PERCPU_H
#define _JAILHOUSE_PERCPU_H

/**
 * @defgroup Per-CPU Per-CPU Subsystem
 *
 * The per-CPU subsystem provides a CPU-local state structure and accessors.
 */

#include <jailhouse/cell.h>
#include <asm/percpu.h>

/**
 * @ingroup Per-CPU
 * @{
 */

/** Per-CPU states accessible across all CPUs. */
struct public_per_cpu {
	/** Per-CPU root page table. Public because it has to be accessible for
	 *  page walks at any time. */
	u8 root_table_page[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

	ARCH_PUBLIC_PERCPU_FIELDS;
} __attribute__((aligned(PAGE_SIZE)));

/** Per-CPU states. */
struct per_cpu {
	/* Must be first field! */
	union {
		/** Stack used while in hypervisor mode. */
		u8 stack[STACK_SIZE];
		struct {
			u8 __fill[STACK_SIZE - sizeof(union registers)];
			/** Guest registers saved on stack during VM exit. */
			union registers guest_regs;
		};
	};

	/** Per-CPU paging structures. */
	struct paging_structures pg_structs;

	/** Logical CPU ID (same as Linux). */
	unsigned int cpu_id;
	/** Owning cell. */
	struct cell *cell;

	/** Statistic counters. */
	u32 stats[JAILHOUSE_NUM_CPU_STATS];

	/** State of the shutdown process. Possible values:
	 * @li SHUTDOWN_NONE: no shutdown in progress
	 * @li SHUTDOWN_STARTED: shutdown in progress
	 * @li negative error code: shutdown failed
	 */
	int shutdown_state;
	/** True if CPU violated a cell boundary or cause some other failure in
	 *  guest mode. */
	bool failed;

	/** Set to true for instructing the CPU to suspend. */
	volatile bool suspend_cpu;
	/** True if CPU is suspended. */
	volatile bool cpu_suspended;
	/** Set to true for a pending TLB flush for the paging layer that does
	 *  host physical <-> guest physical memory mappings. */
	bool flush_vcpu_caches;

	ARCH_PERCPU_FIELDS;

	/* Must be last field! */
	struct public_per_cpu public;
} __attribute__((aligned(PAGE_SIZE)));

/**
 * Retrieve the data structure of the current CPU.
 *
 * @return Pointer to per-CPU data structure.
 */
static inline struct per_cpu *this_cpu_data(void)
{
	return (struct per_cpu *)LOCAL_CPU_BASE;
}

/**
 * Retrieve the publicly accessible data structure of the current CPU.
 *
 * @return Pointer to public per-CPU data structure.
 */
static inline struct public_per_cpu *this_cpu_public(void)
{
	return &this_cpu_data()->public;
}

/**
 * Retrieve the ID of the current CPU.
 *
 * @return CPU ID.
 */
static inline unsigned int this_cpu_id(void)
{
	return this_cpu_data()->cpu_id;
}

/**
 * Retrieve the cell owning the current CPU.
 *
 * @return Pointer to cell.
 */
static inline struct cell *this_cell(void)
{
	return this_cpu_data()->cell;
}

/**
 * Retrieve the data structure of the specified CPU.
 * @param cpu	ID of the target CPU.
 *
 * @return Pointer to per-CPU data structure.
 */
static inline struct per_cpu *per_cpu(unsigned int cpu)
{
	extern u8 __page_pool[];

	return (struct per_cpu *)(__page_pool + cpu * sizeof(struct per_cpu));
}

/**
 * Retrieve the publicly accessible data structure of the specified CPU.
 * @param cpu	ID of the target CPU.
 *
 * @return Pointer to public per-CPU data structure.
 */
static inline struct public_per_cpu *public_per_cpu(unsigned int cpu)
{
	return &per_cpu(cpu)->public;
}

/** @} **/

#endif /* !_JAILHOUSE_PERCPU_H */
