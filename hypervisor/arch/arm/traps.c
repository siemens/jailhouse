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

static void access_cell_reg(struct trap_context *ctx, u8 reg,
				unsigned long *val, bool is_read)
{
	unsigned long mode = ctx->cpsr & PSR_MODE_MASK;

	switch (reg) {
	case 0 ... 7:
		access_usr_reg(ctx, reg, val, is_read);
		break;
	case 8 ... 12:
		if (mode == PSR_FIQ_MODE)
			access_fiq_reg(reg, val, is_read);
		else
			access_usr_reg(ctx, reg, val, is_read);
		break;
	case 13 ... 14:
		switch (mode) {
		case PSR_USR_MODE:
		case PSR_SYS_MODE:
			/*
			 * lr is saved on the stack, as it is not banked in HYP
			 * mode. sp is banked, so lr is at offset 13 in the USR
			 * regs.
			 */
			if (reg == 13)
				access_banked_reg(usr, reg, val, is_read);
			else
				access_usr_reg(ctx, 13, val, is_read);
			break;
		case PSR_SVC_MODE:
			access_banked_reg(svc, reg, val, is_read);
			break;
		case PSR_UND_MODE:
			access_banked_reg(und, reg, val, is_read);
			break;
		case PSR_ABT_MODE:
			access_banked_reg(abt, reg, val, is_read);
			break;
		case PSR_IRQ_MODE:
			access_banked_reg(irq, reg, val, is_read);
			break;
		case PSR_FIQ_MODE:
			access_banked_reg(fiq, reg, val, is_read);
			break;
		}
		break;
	case 15:
		/*
		 * A trapped instruction that accesses the PC? Probably a bug,
		 * but nothing seems to prevent it.
		 */
		printk("WARNING: trapped instruction attempted to explicitly "
		       "access the PC.\n");
		if (is_read)
			*val = ctx->pc;
		else
			ctx->pc = *val;
		break;
	default:
		/* Programming error */
		printk("ERROR: attempt to write register %d\n", reg);
		break;
	}
}

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

	arm_read_banked_reg(ELR_hyp, ctx.pc);
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

	arm_write_banked_reg(ELR_hyp, ctx.pc);
}
