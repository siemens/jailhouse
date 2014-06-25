/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_CONTROL_H
#define _JAILHOUSE_ASM_CONTROL_H

#include <asm/cell.h>
#include <asm/percpu.h>

#ifndef __ASSEMBLY__

int arch_mmu_cell_init(struct cell *cell);
int arch_mmu_cpu_cell_init(struct per_cpu *cpu_data);
void arch_handle_sgi(struct per_cpu *cpu_data, u32 irqn);
void arch_handle_trap(struct per_cpu *cpu_data, struct registers *guest_regs);
void arch_handle_exit(struct per_cpu *cpu_data, struct registers *guest_regs);

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_CONTROL_H */
