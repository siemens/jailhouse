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

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <asm/control.h>
#include <asm/irqchip.h>
#include <asm/traps.h>

int arch_cell_create(struct cell *cell)
{
	return trace_error(-EINVAL);
}

void arch_flush_cell_vcpu_caches(struct cell *cell)
{
	/* AARCH64_TODO */
	trace_error(-EINVAL);
}

void arch_cell_destroy(struct cell *cell)
{
	trace_error(-EINVAL);
	while (1);
}

void arch_cell_reset(struct cell *cell)
{
}

void arch_config_commit(struct cell *cell_added_removed)
{
}

void arch_shutdown(void)
{
	trace_error(-EINVAL);
	while (1);
}

void arch_suspend_cpu(unsigned int cpu_id)
{
	trace_error(-EINVAL);
	while (1);
}

void arch_resume_cpu(unsigned int cpu_id)
{
	trace_error(-EINVAL);
	while (1);
}

void arch_reset_cpu(unsigned int cpu_id)
{
	trace_error(-EINVAL);
	while (1);
}

void arch_park_cpu(unsigned int cpu_id)
{
	trace_error(-EINVAL);
	while (1);
}

void __attribute__((noreturn)) arch_panic_stop(void)
{
	trace_error(-EINVAL);
	while (1);
}

void arch_panic_park(void)
{
	trace_error(-EINVAL);
	while (1);
}

void arch_handle_sgi(struct per_cpu *cpu_data, u32 irqn,
		     unsigned int count_event)
{
	trace_error(-EINVAL);
	while (1);
}

bool arch_handle_phys_irq(struct per_cpu *cpu_data, u32 irqn,
			  unsigned int count_event)
{
	trace_error(-EINVAL);
	while (1);
}

/*
 * We get rid of the virt_id in the AArch64 implementation, since it
 * doesn't really fit with the MPIDR CPU identification scheme on ARM.
 *
 * Until the GICv3 and ARMv7 code has been properly refactored to
 * support this scheme, we stub this call so we can share the GICv2
 * code with ARMv7.
 *
 * TODO: implement MPIDR support in the GICv3 code, so it can be
 * used on AArch64.
 * TODO: refactor out virt_id from the AArch7 port as well.
 */
unsigned int arm_cpu_phys2virt(unsigned int cpu_id)
{
	panic_printk("FATAL: we shouldn't reach here\n");
	panic_stop();

	return -EINVAL;
}
