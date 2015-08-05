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
 *
 * Condition check code is copied from Linux's
 * - arch/arm/kernel/opcodes.c
 * - arch/arm/kvm/emulate.c
 */

#include <asm/control.h>
#include <asm/gic_common.h>
#include <asm/platform.h>
#include <asm/psci.h>
#include <asm/traps.h>
#include <asm/sysregs.h>
#include <jailhouse/printk.h>
#include <jailhouse/control.h>

/*
 * condition code lookup table
 * index into the table is test code: EQ, NE, ... LT, GT, AL, NV
 *
 * bit position in short is condition code: NZCV
 */
static const unsigned short cc_map[16] = {
	0xF0F0,			/* EQ == Z set            */
	0x0F0F,			/* NE                     */
	0xCCCC,			/* CS == C set            */
	0x3333,			/* CC                     */
	0xFF00,			/* MI == N set            */
	0x00FF,			/* PL                     */
	0xAAAA,			/* VS == V set            */
	0x5555,			/* VC                     */
	0x0C0C,			/* HI == C set && Z clear */
	0xF3F3,			/* LS == C clear || Z set */
	0xAA55,			/* GE == (N==V)           */
	0x55AA,			/* LT == (N!=V)           */
	0x0A05,			/* GT == (!Z && (N==V))   */
	0xF5FA,			/* LE == (Z || (N!=V))    */
	0xFFFF,			/* AL always              */
	0			/* NV                     */
};

/* Check condition field either from ESR or from SPSR in thumb mode */
static bool arch_failed_condition(struct trap_context *ctx)
{
	u32 class = ESR_EC(ctx->esr);
	u32 icc = ESR_ICC(ctx->esr);
	u32 cpsr = ctx->cpsr;
	u32 flags = cpsr >> 28;
	u32 cond;
	/*
	 * Trapped instruction is unconditional, already passed the condition
	 * check, or is invalid
	 */
	if (class & 0x30 || class == 0)
		return false;

	/* Is condition field valid? */
	if (icc & ESR_ICC_CV_BIT) {
		cond = ESR_ICC_COND(icc);
	} else {
		/* This can happen in Thumb mode: examine IT state. */
		unsigned long it = PSR_IT(cpsr);

		/* it == 0 => unconditional. */
		if (it == 0)
			return false;

		/* The cond for this insn works out as the top 4 bits. */
		cond = (it >> 4);
	}

	/* Compare the apsr flags with the condition code */
	if ((cc_map[cond] >> flags) & 1)
		return false;

	return true;
}

/*
 * When exceptions occur while instructions are executed in Thumb IF-THEN
 * blocks, the ITSTATE field of the CPSR is not advanced (updated), so we have
 * to do this little bit of work manually. The fields map like this:
 *
 * IT[7:0] -> CPSR[26:25],CPSR[15:10]
 */
static void arch_advance_itstate(struct trap_context *ctx)
{
	unsigned long itbits, cond;
	unsigned long cpsr = ctx->cpsr;

	if (!(cpsr & PSR_IT_MASK(0xff)))
		return;

	itbits = PSR_IT(cpsr);
	cond = itbits >> 5;

	if ((itbits & 0x7) == 0)
		/* One instruction left in the block, next itstate is 0 */
		itbits = cond = 0;
	else
		itbits = (itbits << 1) & 0x1f;

	itbits |= (cond << 5);
	cpsr &= ~PSR_IT_MASK(0xff);
	cpsr |= PSR_IT_MASK(itbits);

	ctx->cpsr = cpsr;
}

void arch_skip_instruction(struct trap_context *ctx)
{
	u32 instruction_length = ESR_IL(ctx->esr);

	ctx->pc += (instruction_length ? 4 : 2);
	arch_advance_itstate(ctx);
}

void access_cell_reg(struct trap_context *ctx, u8 reg, unsigned long *val,
		     bool is_read)
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

static void dump_guest_regs(struct trap_context *ctx)
{
	u8 reg;
	unsigned long reg_val;

	panic_printk("pc=0x%08x cpsr=0x%08x esr=0x%08x\n", ctx->pc, ctx->cpsr,
			ctx->esr);
	for (reg = 0; reg < 15; reg++) {
		access_cell_reg(ctx, reg, &reg_val, true);
		panic_printk("r%d=0x%08x ", reg, reg_val);
		if ((reg + 1) % 4 == 0)
			panic_printk("\n");
	}
	panic_printk("\n");
}

static int arch_handle_smc(struct trap_context *ctx)
{
	unsigned long *regs = ctx->regs;

	if (IS_PSCI_FN(regs[0]))
		regs[0] = psci_dispatch(ctx);
	else
		regs[0] = smc(regs[0], regs[1], regs[2], regs[3]);

	arch_skip_instruction(ctx);

	return TRAP_HANDLED;
}

static int arch_handle_hvc(struct trap_context *ctx)
{
	unsigned long *regs = ctx->regs;

	if (IS_PSCI_FN(regs[0]))
		regs[0] = psci_dispatch(ctx);
	else
		regs[0] = hypercall(regs[0], regs[1], regs[2]);

	return TRAP_HANDLED;
}

static int arch_handle_cp15_32(struct trap_context *ctx)
{
	u32 opc2	= ctx->esr >> 17 & 0x7;
	u32 opc1	= ctx->esr >> 14 & 0x7;
	u32 crn		= ctx->esr >> 10 & 0xf;
	u32 rt		= ctx->esr >> 5 & 0xf;
	u32 crm		= ctx->esr >> 1 & 0xf;
	u32 read	= ctx->esr & 1;

	if (opc1 == 0 && crn == 1 && crm == 0 && opc2 == 1) {
		/* Do not let the guest disable coherency by writing ACTLR... */
		if (read) {
			unsigned long val;
			arm_read_sysreg(ACTLR_EL1, val);
			access_cell_reg(ctx, rt, &val, false);
		}
		arch_skip_instruction(ctx);

		return TRAP_HANDLED;
	}

	return TRAP_UNHANDLED;
}

static int arch_handle_cp15_64(struct trap_context *ctx)
{
	unsigned long rt_val, rt2_val;
	u32 opc1	= ctx->esr >> 16 & 0x7;
	u32 rt2		= ctx->esr >> 10 & 0xf;
	u32 rt		= ctx->esr >> 5 & 0xf;
	u32 crm		= ctx->esr >> 1 & 0xf;
	u32 read	= ctx->esr & 1;

	if (!read) {
		access_cell_reg(ctx, rt, &rt_val, true);
		access_cell_reg(ctx, rt2, &rt2_val, true);
	}

#ifdef CONFIG_ARM_GIC_V3
	/* Trapped ICC_SGI1R write */
	if (!read && opc1 == 0 && crm == 12) {
		arch_skip_instruction(ctx);
		gicv3_handle_sgir_write((u64)rt2_val << 32 | rt_val);
		return TRAP_HANDLED;
	}
#else
	/* Avoid `unused' warning... */
	crm = crm;
	opc1 = opc1;
#endif

	return TRAP_UNHANDLED;
}

static const trap_handler trap_handlers[38] =
{
	[ESR_EC_CP15_32]	= arch_handle_cp15_32,
	[ESR_EC_CP15_64]	= arch_handle_cp15_64,
	[ESR_EC_HVC]		= arch_handle_hvc,
	[ESR_EC_SMC]		= arch_handle_smc,
	[ESR_EC_DABT]		= arch_handle_dabt,
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

	/*
	 * On some implementations, instructions that fail their condition check
	 * can trap.
	 */
	if (arch_failed_condition(&ctx)) {
		arch_skip_instruction(&ctx);
		goto restore_context;
	}

	if (trap_handlers[exception_class])
		ret = trap_handlers[exception_class](&ctx);

	switch (ret) {
	case TRAP_UNHANDLED:
	case TRAP_FORBIDDEN:
		panic_printk("FATAL: %s (exception class 0x%02x)\n",
			     (ret == TRAP_UNHANDLED ? "unhandled trap" :
						      "forbidden access"),
			     exception_class);
		dump_guest_regs(&ctx);
		panic_park();
	}

restore_context:
	arm_write_banked_reg(SPSR_hyp, ctx.cpsr);
	arm_write_banked_reg(ELR_hyp, ctx.pc);
}
