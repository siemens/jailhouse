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
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/control.h>
#include <asm/irqchip.h>
#include <asm/setup.h>

static u32 __attribute__((aligned(PAGE_SIZE))) parking_code[PAGE_SIZE / 4] = {
	0xd503207f, /* 1: wfi  */
	0x17ffffff, /*    b 1b */
};

struct paging_structures parking_mm;

int arch_init_early(void)
{
	int err;

	parking_mm.root_paging = cell_paging;
	parking_mm.root_table =
		page_alloc_aligned(&mem_pool, ARM_CELL_ROOT_PT_SZ);
	if (!parking_mm.root_table)
		return -ENOMEM;

	err = paging_create(&parking_mm, paging_hvirt2phys(parking_code),
			    PAGE_SIZE, 0,
			    (PTE_FLAG_VALID | PTE_ACCESS_FLAG |
			     S2_PTE_ACCESS_RO | S2_PTE_FLAG_NORMAL),
			    PAGING_COHERENT);
	if (err)
		return err;

	return arm_paging_cell_init(&root_cell);
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	int err;

	/* switch to the permanent page tables */
	enable_mmu_el2(hv_paging_structs.root_table);

	cpu_data->mpidr = phys_processor_id();

	arm_paging_vcpu_init(&root_cell.arch.mm);

	err = irqchip_init();
	if (err)
		return err;

	return irqchip_cpu_init(cpu_data);
}

int arch_init_late(void)
{
	int err;

	/* Setup the SPI bitmap */
	err = irqchip_cell_init(&root_cell);
	if (err)
		return err;

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
