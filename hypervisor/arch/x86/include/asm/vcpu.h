/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_VCPU_H
#define _JAILHOUSE_ASM_VCPU_H

#include <jailhouse/entry.h>
#include <jailhouse/cell-config.h>
#include <jailhouse/paging.h>
#include <asm/cell.h>
#include <asm/percpu.h>
#include <asm/processor.h>

int vcpu_vendor_init(void);

int vcpu_cell_init(struct cell *cell);
int vcpu_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem);
int vcpu_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem);
void vcpu_cell_exit(struct cell *cell);

int vcpu_init(struct per_cpu *cpu_data);
void vcpu_exit(struct per_cpu *cpu_data);

void __attribute__((noreturn)) vcpu_activate_vmm(struct per_cpu *cpu_data);
void vcpu_handle_exit(struct registers *guest_regs, struct per_cpu *cpu_data);

void vcpu_park(struct per_cpu *cpu_data);

void vcpu_nmi_handler(struct per_cpu *cpu_data);

void vcpu_tlb_flush(void);

void vcpu_entry_failure(struct per_cpu *cpu_data);

/*
 * vcpu_map_inst() and vcpu_get_inst_bytes() contract:
 *
 * On input, *size gives the number of bytes to get.
 * On output, *size is the number of bytes available.
 *
 * If the function fails (returns NULL), *size is undefined.
 */

const u8 *vcpu_map_inst(const struct guest_paging_structures *pg_structs,
			unsigned long pc, unsigned int *size);

const u8 *vcpu_get_inst_bytes(const struct guest_paging_structures *pg_structs,
			      unsigned long pc, unsigned int *size);

void vcpu_skip_emulated_instruction(unsigned int inst_len);

bool vcpu_get_guest_paging_structs(struct guest_paging_structures *pg_structs);

#endif
