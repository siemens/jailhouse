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
#include <jailhouse/entry.h>
#include <jailhouse/printk.h>
#include <asm/control.h>
#include <asm/setup.h>

int arch_init_early(void)
{
	return arm_paging_cell_init(&root_cell);
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	/* switch to the permanent page tables */
	enable_mmu_el2(hv_paging_structs.root_table);

	arm_paging_vcpu_init(&root_cell.arch.mm);

	return 0;
}

int arch_init_late(void)
{
	return map_root_memory_regions();
}

void __attribute__((noreturn)) arch_cpu_activate_vmm(struct per_cpu *cpu_data)
{
	trace_error(-EINVAL);
	while (1);
}

void arch_cpu_restore(struct per_cpu *cpu_data, int return_code)
{
	trace_error(-EINVAL);
	while (1);
}
