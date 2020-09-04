/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/processor.h>
#include <asm/setup.h>
#include <asm/smccc.h>

static u32 __attribute__((aligned(PAGE_SIZE))) parking_code[PAGE_SIZE / 4] = {
	ARM_PARKING_CODE
};

int arm_init_early(void)
{
	int err;

	parking_pt.root_paging = cell_paging;

	err = paging_create(&parking_pt, paging_hvirt2phys(parking_code),
			    PAGE_SIZE, 0,
			    (PTE_FLAG_VALID | PTE_ACCESS_FLAG |
			     S2_PTE_ACCESS_RO | S2_PTE_FLAG_NORMAL),
			    PAGING_COHERENT | PAGING_NO_HUGE);
	if (err)
		return err;

	return arm_paging_cell_init(&root_cell);
}

int arm_cpu_init(struct per_cpu *cpu_data)
{
	int err;

	cpu_data->public.mpidr = phys_processor_id();

	arm_write_sysreg(VTCR_EL2, VTCR_CELL);
	arm_paging_vcpu_init(&root_cell.arch.mm);

	err = smccc_discover();
	if (err)
		return err;

	return irqchip_cpu_init(cpu_data);
}
