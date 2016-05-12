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
