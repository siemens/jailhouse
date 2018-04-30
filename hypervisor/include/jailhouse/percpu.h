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

#include <asm/percpu.h>

/**
 * @ingroup Per-CPU
 * @{
 */

/** Per-CPU states. */
struct per_cpu {
	ARCH_PERCPU_FIELDS;
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

/** @} **/

#endif /* !_JAILHOUSE_PERCPU_H */
