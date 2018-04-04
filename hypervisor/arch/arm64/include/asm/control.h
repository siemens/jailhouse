/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_CONTROL_H
#define _JAILHOUSE_ASM_CONTROL_H

#define SGI_INJECT	0
#define SGI_EVENT	1

#include <asm/percpu.h>

void arch_handle_sgi(struct per_cpu *cpu_data, u32 irqn,
		     unsigned int count_event);
struct registers* arch_handle_exit(struct per_cpu *cpu_data,
				   struct registers *regs);
bool arch_handle_phys_irq(struct per_cpu *cpu_data, u32 irqn,
			  unsigned int count_event);
void arch_reset_self(struct per_cpu *cpu_data);
void arch_shutdown_self(struct per_cpu *cpu_data);
unsigned int arm_cpu_by_mpidr(struct cell *cell, unsigned long mpidr);

void arm_cpu_reset(unsigned long pc);
void arm_cpu_park(void);
void arm_cpu_kick(unsigned int cpu_id);

#endif /* !_JAILHOUSE_ASM_CONTROL_H */
