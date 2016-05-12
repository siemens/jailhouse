/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <asm/control.h>
#include <asm/gic.h>
#include <asm/psci.h>
#include <asm/sysregs.h>
#include <asm/traps.h>
#include <asm/processor.h>

void arch_skip_instruction(struct trap_context *ctx)
{
	trace_error(-EINVAL);
	while(1);
}

static void dump_regs(struct trap_context *ctx)
{
	unsigned char i;
	u64 pc;

	arm_read_sysreg(ELR_EL2, pc);
	panic_printk(" pc: %016lx   lr: %016lx spsr: %08lx     EL%1d\n"
		     " sp: %016lx  esr: %02x %01x %07lx\n",
		     pc, ctx->regs[30], ctx->spsr, SPSR_EL(ctx->spsr),
		     ctx->sp, ESR_EC(ctx->esr), ESR_IL(ctx->esr),
		     ESR_ISS(ctx->esr));
	for (i = 0; i < NUM_USR_REGS - 1; i++)
		panic_printk("%sx%d: %016lx%s", i < 10 ? " " : "", i,
			     ctx->regs[i], i % 3 == 2 ? "\n" : "  ");
	panic_printk("\n");
}

static void fill_trap_context(struct trap_context *ctx, struct registers *regs)
{
	arm_read_sysreg(SPSR_EL2, ctx->spsr);
	switch (SPSR_EL(ctx->spsr)) {	/* exception level */
	case 0:
		arm_read_sysreg(SP_EL0, ctx->sp); break;
	case 1:
		arm_read_sysreg(SP_EL1, ctx->sp); break;
	case 2:
		arm_read_sysreg(SP_EL2, ctx->sp); break;
	default:
		ctx->sp = 0; break;	/* should never happen */
	}
	arm_read_sysreg(ESR_EL2, ctx->esr);
	ctx->regs = regs->usr;
}

static void arch_handle_trap(struct per_cpu *cpu_data,
			     struct registers *guest_regs)
{
	struct trap_context ctx;
	int ret;

	fill_trap_context(&ctx, guest_regs);

	/* exception class */
	switch (ESR_EC(ctx.esr)) {
	default:
		ret = TRAP_UNHANDLED;
	}

	if (ret == TRAP_UNHANDLED || ret == TRAP_FORBIDDEN) {
		panic_printk("\nFATAL: exception %s\n", (ret == TRAP_UNHANDLED ?
							 "unhandled trap" :
							 "forbidden access"));
		panic_printk("Cell state before exception:\n");
		dump_regs(&ctx);
		panic_park();
	}
}

static void arch_dump_exit(struct registers *regs, const char *reason)
{
	struct trap_context ctx;

	fill_trap_context(&ctx, regs);
	panic_printk("\nFATAL: Unhandled HYP exception: %s\n", reason);
	dump_regs(&ctx);
}

struct registers *arch_handle_exit(struct per_cpu *cpu_data,
				   struct registers *regs)
{
	cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_TOTAL]++;

	switch (regs->exit_reason) {
	case EXIT_REASON_EL1_ABORT:
		arch_handle_trap(cpu_data, regs);
		break;

	case EXIT_REASON_EL2_ABORT:
		arch_dump_exit(regs, "synchronous abort from EL2");
		panic_stop();
		break;

	default:
		arch_dump_exit(regs, "unexpected");
		panic_stop();
	}

	vmreturn(regs);
}
