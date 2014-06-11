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

#include <jailhouse/entry.h>

int arch_init_early(void)
{
	return -ENOSYS;
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	return -ENOSYS;
}

int arch_init_late(void)
{
	return -ENOSYS;
}

void arch_cpu_activate_vmm(struct per_cpu *cpu_data)
{
	while (1);
}

void arch_cpu_restore(struct per_cpu *cpu_data)
{
}

// catch missing symbols
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <jailhouse/control.h>
#include <jailhouse/string.h>
#include <jailhouse/paging.h>
void arch_dbg_write_init(void) {}
void arch_suspend_cpu(unsigned int cpu_id) {}
void arch_resume_cpu(unsigned int cpu_id) {}
void arch_reset_cpu(unsigned int cpu_id) {}
void arch_park_cpu(unsigned int cpu_id) {}
void arch_shutdown_cpu(unsigned int cpu_id) {}
int arch_cell_create(struct cell *new_cell)
{ return -ENOSYS; }
int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{ return -ENOSYS; }
int arch_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{ return -ENOSYS; }
void arch_flush_cell_vcpu_caches(struct cell *cell) {}
void arch_cell_destroy(struct cell *new_cell) {}
void arch_config_commit(struct cell *cell_added_removed) {}
void arch_dbg_write(const char *msg) {}
void arch_shutdown(void) {}
unsigned long arch_paging_gphys2phys(struct per_cpu *cpu_data,
				     unsigned long gphys, unsigned long flags)
{ return INVALID_PHYS_ADDR; }
void arch_paging_init(void) { }

const struct paging arm_paging[1];

void arch_panic_stop(void) {__builtin_unreachable();}
void arch_panic_park(void) {}
