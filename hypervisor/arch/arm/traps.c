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

#include <asm/control.h>
#include <asm/traps.h>
#include <asm/sysregs.h>
#include <jailhouse/printk.h>
#include <jailhouse/control.h>

static int arch_handle_hvc(struct per_cpu *cpu_data, struct trap_context *ctx)
{
	unsigned long *regs = ctx->regs;

	regs[0] = hypercall(regs[0], regs[1], regs[2]);

	return TRAP_HANDLED;
}

static const trap_handler trap_handlers[38] =
{
	[ESR_EC_HVC]		= arch_handle_hvc,
};

void arch_handle_trap(struct per_cpu *cpu_data, struct registers *guest_regs)
{
	struct trap_context ctx;
	u32 exception_class;
	int ret = TRAP_UNHANDLED;

	arm_read_banked_reg(SPSR_hyp, ctx.cpsr);
	arm_read_sysreg(ESR_EL2, ctx.esr);
	exception_class = ESR_EC(ctx.esr);
	ctx.regs = guest_regs->usr;

	if (trap_handlers[exception_class])
		ret = trap_handlers[exception_class](cpu_data, &ctx);

	if (ret != TRAP_HANDLED) {
		panic_printk("CPU%d: Unhandled HYP trap, syndrome 0x%x\n",
				cpu_data->cpu_id, ctx.esr);
		while(1);
	}
}
