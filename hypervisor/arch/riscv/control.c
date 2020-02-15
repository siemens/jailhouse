/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2020
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>

int arch_cell_create(struct cell *cell)
{
	return -ENOSYS;
}

int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	return -ENOSYS;
}

int arch_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	return -ENOSYS;
}

void arch_flush_cell_vcpu_caches(struct cell *cell)
{
}

void arch_cell_destroy(struct cell *cell)
{
}

void arch_cell_reset(struct cell *cell)
{
}

void arch_config_commit(struct cell *cell_added_removed)
{
}

void arch_prepare_shutdown(void)
{
}

void __attribute__((noreturn)) arch_panic_stop(void)
{
	while (1);
}

void arch_panic_park(void)
{
}

// Likely misplaced
#include <jailhouse/processor.h>
int cpu_by_phys_processor_id(u64 phys_id)
{
	return -1;
}
