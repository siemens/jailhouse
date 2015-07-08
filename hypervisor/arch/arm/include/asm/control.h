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

#define SGI_INJECT	0
#define SGI_CPU_OFF	1

#define CACHES_CLEAN		0
#define CACHES_CLEAN_INVALIDATE	1

#ifndef __ASSEMBLY__

#include <jailhouse/cell.h>
#include <asm/percpu.h>

void arch_cpu_dcaches_flush(unsigned int action);
void arch_cpu_icache_flush(void);
void arch_cpu_tlb_flush(struct per_cpu *cpu_data);
void arch_cell_caches_flush(struct cell *cell);
int arch_mmu_cell_init(struct cell *cell);
void arch_mmu_cell_destroy(struct cell *cell);
int arch_mmu_cpu_cell_init(struct per_cpu *cpu_data);
void arch_handle_sgi(struct per_cpu *cpu_data, u32 irqn);
void arch_handle_trap(struct per_cpu *cpu_data, struct registers *guest_regs);
struct registers* arch_handle_exit(struct per_cpu *cpu_data,
				   struct registers *regs);
bool arch_handle_phys_irq(struct per_cpu *cpu_data, u32 irqn);
void arch_reset_self(struct per_cpu *cpu_data);
void arch_shutdown_self(struct per_cpu *cpu_data);

void __attribute__((noreturn)) vmreturn(struct registers *guest_regs);
void __attribute__((noreturn)) arch_shutdown_mmu(struct per_cpu *cpu_data);

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_CONTROL_H */
