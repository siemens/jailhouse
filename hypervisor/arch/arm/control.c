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

#include <jailhouse/printk.h>
#include <asm/control.h>
#include <asm/irqchip.h>

void arch_handle_sgi(struct per_cpu *cpu_data, u32 irqn)
{
	switch (irqn) {
	case SGI_INJECT:
		irqchip_inject_pending(cpu_data);
		break;
	}
}

void arch_handle_exit(struct per_cpu *cpu_data, struct registers *regs)
{
	switch (regs->exit_reason) {
	case EXIT_REASON_IRQ:
		irqchip_handle_irq(cpu_data);
		break;
	case EXIT_REASON_TRAP:
		arch_handle_trap(cpu_data, regs);
		break;
	default:
		printk("Internal error: %d exit not implemented\n",
				regs->exit_reason);
		while(1);
	}
}
