/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * Based on vmx.c written by Jan Kiszka.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/entry.h>
#include <jailhouse/cell-config.h>
#include <jailhouse/paging.h>
#include <asm/cell.h>
#include <asm/paging.h>
#include <asm/percpu.h>
#include <asm/processor.h>
#include <asm/svm.h>
#include <asm/vcpu.h>

unsigned long arch_paging_gphys2phys(struct per_cpu *cpu_data,
				     unsigned long gphys,
				     unsigned long flags)
{
	/* TODO: Implement */
	return INVALID_PHYS_ADDR;
}

int vcpu_vendor_init(void)
{
	/* TODO: Implement */

	return 0;
}

int vcpu_vendor_cell_init(struct cell *cell)
{
	/* TODO: Implement */
	return 0;
}

int vcpu_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	/* TODO: Implement */
	return 0;
}

int vcpu_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	/* TODO: Implement */
	return 0;
}

void vcpu_vendor_cell_exit(struct cell *cell)
{
	/* TODO: Implement */
}

int vcpu_init(struct per_cpu *cpu_data)
{
	/* TODO: Implement */
	return 0;
}

void vcpu_exit(struct per_cpu *cpu_data)
{
	/* TODO: Implement */
}

void vcpu_activate_vmm(struct per_cpu *cpu_data)
{
	/* TODO: Implement */
	__builtin_unreachable();
}

void __attribute__((noreturn))
vcpu_deactivate_vmm(struct registers *guest_regs)
{
	/* TODO: Implement */
	__builtin_unreachable();
}

void vcpu_skip_emulated_instruction(unsigned int inst_len)
{
	/* TODO: Implement */
}

bool vcpu_get_guest_paging_structs(struct guest_paging_structures *pg_structs)
{
	/* TODO: Implement */
	return false;
}

void vcpu_handle_exit(struct registers *guest_regs, struct per_cpu *cpu_data)
{
	/* TODO: Implement */
}

void vcpu_park(struct per_cpu *cpu_data)
{
	/* TODO: Implement */
}

void vcpu_nmi_handler(struct per_cpu *cpu_data)
{
	/* TODO: Implement */
}

void vcpu_tlb_flush(void)
{
	/* TODO: Implement */
}

const u8 *vcpu_get_inst_bytes(const struct guest_paging_structures *pg_structs,
			      unsigned long pc, unsigned int *size)
{
	/* TODO: Implement */
	return NULL;
}

void vcpu_vendor_get_cell_io_bitmap(struct cell *cell,
		                    struct vcpu_io_bitmap *out)
{
	/* TODO: Implement */
}

void vcpu_vendor_get_execution_state(struct vcpu_execution_state *x_state)
{
	/* TODO: Implement */
}
