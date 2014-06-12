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

#include <asm/setup.h>
#include <asm/sysregs.h>
#include <jailhouse/entry.h>
#include <jailhouse/paging.h>
#include <jailhouse/string.h>

int arch_init_early(void)
{
	return -ENOSYS;
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	int err = 0;

	/*
	 * Copy the registers to restore from the linux stack here, because we
	 * won't be able to access it later
	 */
	memcpy(&cpu_data->linux_reg, (void *)cpu_data->linux_sp, NUM_ENTRY_REGS
			* sizeof(unsigned long));

	err = switch_exception_level(cpu_data);

	/*
	 * Save pointer in the thread local storage
	 * Must be done early in order to handle aborts and errors in the setup
	 * code.
	 */
	arm_write_sysreg(TPIDR_EL2, cpu_data);

	return err;
}

int arch_init_late(void)
{
	return -ENOSYS;
}

void arch_cpu_activate_vmm(struct per_cpu *cpu_data)
{
	/* Return to the kernel */
	cpu_return_el1(cpu_data);

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
void arch_shutdown(void) {}
unsigned long arch_paging_gphys2phys(struct per_cpu *cpu_data,
				     unsigned long gphys, unsigned long flags)
{ return INVALID_PHYS_ADDR; }
void arch_panic_stop(void) {__builtin_unreachable();}
void arch_panic_park(void) {}
