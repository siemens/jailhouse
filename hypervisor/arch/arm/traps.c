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

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <asm/control.h>
#include <asm/gic.h>
#include <asm/psci.h>
#include <asm/traps.h>
#include <asm/sysregs.h>

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

/* Check condition field either from HSR or from SPSR in thumb mode */
static bool arch_failed_condition(struct trap_context *ctx)
{
	u32 class = HSR_EC(ctx->hsr);
	u32 iss = HSR_ISS(ctx->hsr);
	u32 cpsr, flags, cond;

	arm_read_banked_reg(SPSR_hyp, cpsr);
	flags = cpsr >> 28;

	/*
	 * Trapped instruction is unconditional, already passed the condition
	 * check, or is invalid
	 */
	if (class & 0x30 || class == 0)
		return false;

	/* Is condition field valid? */
	if (iss & HSR_ISS_CV_BIT) {
		cond = HSR_ISS_COND(iss);
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
	u32 cpsr;

	arm_read_banked_reg(SPSR_hyp, cpsr);
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

	arm_write_banked_reg(SPSR_hyp, cpsr);
}

void arch_skip_instruction(struct trap_context *ctx)
{
	u32 pc;

	arm_read_banked_reg(ELR_hyp, pc);
	pc += HSR_IL(ctx->hsr) ? 4 : 2;
	arm_write_banked_reg(ELR_hyp, pc);
	arch_advance_itstate(ctx);
}

static inline void access_usr_reg(struct trap_context *ctx, u8 reg,
				  unsigned long *val, bool is_read)
{
	if (is_read)
		*val = ctx->regs[reg];
	else
		ctx->regs[reg] = *val;
}

#define access_banked_r13_r14(mode, reg, val, is_read)			\
	do {								\
		if (reg == 13)						\
			arm_rw_banked_reg(SP_##mode, *val, is_read);	\
		else							\
			arm_rw_banked_reg(LR_##mode, *val, is_read);	\
	} while (0)

void access_cell_reg(struct trap_context *ctx, u8 reg, unsigned long *val,
		     bool is_read)
{
	u32 mode;

	arm_read_banked_reg(SPSR_hyp, mode);
	mode &= PSR_MODE_MASK;

	switch (reg) {
	case 0 ... 12:
		if (reg >= 8 && mode == PSR_FIQ_MODE)
			switch (reg) {
			case 8:
				arm_rw_banked_reg(r8_fiq, *val, is_read);
				break;
			case 9:
				arm_rw_banked_reg(r9_fiq, *val, is_read);
				break;
			case 10:
				arm_rw_banked_reg(r10_fiq, *val, is_read);
				break;
			case 11:
				arm_rw_banked_reg(r11_fiq, *val, is_read);
				break;
			case 12:
				arm_rw_banked_reg(r12_fiq, *val, is_read);
				break;
			}
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
				access_banked_r13_r14(usr, reg, val, is_read);
			else
				access_usr_reg(ctx, 13, val, is_read);
			break;
		case PSR_SVC_MODE:
			access_banked_r13_r14(svc, reg, val, is_read);
			break;
		case PSR_UND_MODE:
			access_banked_r13_r14(und, reg, val, is_read);
			break;
		case PSR_ABT_MODE:
			access_banked_r13_r14(abt, reg, val, is_read);
			break;
		case PSR_IRQ_MODE:
			access_banked_r13_r14(irq, reg, val, is_read);
			break;
		case PSR_FIQ_MODE:
			access_banked_r13_r14(fiq, reg, val, is_read);
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
			arm_read_banked_reg(ELR_hyp, *val);
		else
			arm_write_banked_reg(ELR_hyp, *val);
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
	u32 pc, cpsr;

	arm_read_banked_reg(ELR_hyp, pc);
	arm_read_banked_reg(SPSR_hyp, cpsr);
	panic_printk("pc=0x%08x cpsr=0x%08x hsr=0x%08x\n", pc, cpsr, ctx->hsr);
	for (reg = 0; reg < 15; reg++) {
		access_cell_reg(ctx, reg, &reg_val, true);
		panic_printk("r%d=0x%08lx ", reg, reg_val);
		if ((reg + 1) % 4 == 0)
			panic_printk("\n");
	}
	panic_printk("\n");
}

static int arch_handle_smc(struct trap_context *ctx)
{
	unsigned long *regs = ctx->regs;

	if (!IS_PSCI_32(regs[0]) && !IS_PSCI_UBOOT(regs[0]))
		return TRAP_FORBIDDEN;

	regs[0] = psci_dispatch(ctx);

	arch_skip_instruction(ctx);

	return TRAP_HANDLED;
}

static int arch_handle_hvc(struct trap_context *ctx)
{
	unsigned long *regs = ctx->regs;
	unsigned long code = regs[0];

	if (HSR_ISS(ctx->hsr) != JAILHOUSE_HVC_CODE)
		return TRAP_FORBIDDEN;

	regs[0] = hypercall(code, regs[1], regs[2]);

	if (code == JAILHOUSE_HC_DISABLE && regs[0] == 0)
		arch_shutdown_self(this_cpu_data());

	return TRAP_HANDLED;
}

static int arch_handle_cp15_32(struct trap_context *ctx)
{
	u32 hsr = ctx->hsr;
	u32 rt = (hsr >> 5) & 0xf;
	u32 read = hsr & 1;
	u32 hcr, old_sctlr;
	unsigned long val;

#define CP15_32_PERFORM_WRITE(crn, opc1, crm, opc2) ({			\
	bool match = false;						\
	if (HSR_MATCH_MCR_MRC(hsr, crn, opc1, crm, opc2)) {		\
		arm_write_sysreg_32(opc1, c##crn, c##crm, opc2, val);	\
		match = true;						\
	}								\
	match;								\
})

	this_cpu_data()->stats[JAILHOUSE_CPU_STAT_VMEXITS_CP15]++;

	if (!read)
		access_cell_reg(ctx, rt, &val, true);

	/* trapped by HCR.TAC */
	if (HSR_MATCH_MCR_MRC(ctx->hsr, 1, 0, 0, 1)) { /* ACTLR */
		/* Do not let the guest disable coherency by writing ACTLR... */
		if (read)
			arm_read_sysreg(ACTLR_EL1, val);
	}
	/* all other regs are write-only / only trapped on writes */
	else if (read) {
		return TRAP_UNHANDLED;
	}
	/* trapped by HCR.TSW */
	else if (HSR_MATCH_MCR_MRC(hsr, 7, 0, 6, 2) ||  /* DCISW */
		   HSR_MATCH_MCR_MRC(hsr, 7, 0, 10, 2) || /* DCCSW */
		   HSR_MATCH_MCR_MRC(hsr, 7, 0, 14, 2)) { /* DCCISW */
		arm_read_sysreg(HCR, hcr);
		if (!(hcr & HCR_TVM_BIT)) {
			arm_cell_dcaches_flush(this_cell(),
					       DCACHE_CLEAN_AND_INVALIDATE);
			arm_write_sysreg(HCR, hcr | HCR_TVM_BIT);
		}
	}
	/* trapped if HCR.TVM is set */
	else if (HSR_MATCH_MCR_MRC(hsr, 1, 0, 0, 0)) { /* SCTLR */
		arm_read_sysreg(SCTLR_EL1, old_sctlr);

		arm_write_sysreg(SCTLR_EL1, val);

		/* Check if caches were turned on or off. */
		if (SCTLR_C_AND_M_SET(val) != SCTLR_C_AND_M_SET(old_sctlr)) {
			/* Flush dcaches again if they were enabled before. */
			if (SCTLR_C_AND_M_SET(old_sctlr))
				arm_cell_dcaches_flush(this_cell(),
						DCACHE_CLEAN_AND_INVALIDATE);
			/* Stop tracking VM control regs. */
			arm_read_sysreg(HCR, hcr);
			arm_write_sysreg(HCR, hcr & ~HCR_TVM_BIT);
		}
	} else if (!(CP15_32_PERFORM_WRITE(2, 0, 0, 0) ||   /* TTBR0 */
		     CP15_32_PERFORM_WRITE(2, 0, 0, 1) ||   /* TTBR1 */
		     CP15_32_PERFORM_WRITE(2, 0, 0, 2) ||   /* TTBCR */
		     CP15_32_PERFORM_WRITE(3, 0, 0, 0) ||   /* DACR */
		     CP15_32_PERFORM_WRITE(5, 0, 0, 0) ||   /* DFSR */
		     CP15_32_PERFORM_WRITE(5, 0, 0, 1) ||   /* IFSR */
		     CP15_32_PERFORM_WRITE(6, 0, 0, 0) ||   /* DFAR */
		     CP15_32_PERFORM_WRITE(6, 0, 0, 2) ||   /* IFAR */
		     CP15_32_PERFORM_WRITE(5, 0, 1, 0) ||   /* ADFSR */
		     CP15_32_PERFORM_WRITE(5, 0, 1, 1) ||   /* AIDSR */
		     CP15_32_PERFORM_WRITE(10, 0, 2, 0) ||  /* PRRR / MAIR0 */
		     CP15_32_PERFORM_WRITE(10, 0, 2, 1) ||  /* NMRR / MAIR1 */
		     CP15_32_PERFORM_WRITE(13, 0, 0, 1))) { /* CONTEXTIDR */
		return TRAP_UNHANDLED;
	}

	if (read)
		access_cell_reg(ctx, rt, &val, false);

	arch_skip_instruction(ctx);

	return TRAP_HANDLED;
}

static int arch_handle_cp15_64(struct trap_context *ctx)
{
	u32 hsr  = ctx->hsr;
	u32 rt2  = (hsr >> 10) & 0xf;
	u32 rt   = (hsr >> 5) & 0xf;
	u32 read = hsr & 1;
	unsigned long lo, hi;

#define CP15_64_PERFORM_WRITE(opc1, crm) ({		\
	bool match = false;				\
	if (HSR_MATCH_MCRR_MRRC(hsr, opc1, crm)) {	\
		arm_write_sysreg_64(opc1, c##crm, ((u64)hi << 32) | lo); \
		match = true;				\
	}						\
	match;						\
})

	this_cpu_data()->stats[JAILHOUSE_CPU_STAT_VMEXITS_CP15]++;

	/* all regs are write-only / only trapped on writes */
	if (read)
		return TRAP_UNHANDLED;

	access_cell_reg(ctx, rt, &lo, true);
	access_cell_reg(ctx, rt2, &hi, true);

	/* trapped by HCR.IMO/FMO */
	if (HSR_MATCH_MCRR_MRRC(ctx->hsr, 0, 12)) { /* ICC_SGI1R */
		if (!gicv3_handle_sgir_write(((u64)hi << 32) | lo))
			return TRAP_UNHANDLED;
	} else {
		/* trapped if HCR.TVM is set */
		if (!(CP15_64_PERFORM_WRITE(0, 2) ||	/* TTBR0 */
		    CP15_64_PERFORM_WRITE(1, 2)))	/* TTBR1 */
			return TRAP_UNHANDLED;
	}

	arch_skip_instruction(ctx);

	return TRAP_HANDLED;
}

static int handle_iabt(struct trap_context *ctx)
{
	unsigned long hpfar, hdfar;

	arm_read_sysreg(HPFAR, hpfar);
	arm_read_sysreg(HDFAR, hdfar);

	panic_printk("FATAL: instruction abort at 0x%lx\n",
		     (hpfar << 8) | (hdfar & 0xfff));
	return TRAP_FORBIDDEN;
}

static const trap_handler trap_handlers[0x40] =
{
	[HSR_EC_CP15_32]	= arch_handle_cp15_32,
	[HSR_EC_CP15_64]	= arch_handle_cp15_64,
	[HSR_EC_HVC]		= arch_handle_hvc,
	[HSR_EC_SMC]		= arch_handle_smc,
	[HSR_EC_IABT]		= handle_iabt,
	[HSR_EC_DABT]		= arch_handle_dabt,
};

static void arch_handle_trap(struct registers *guest_regs)
{
	struct trap_context ctx;
	u32 exception_class;
	int ret = TRAP_UNHANDLED;

	arm_read_sysreg(HSR, ctx.hsr);
	exception_class = HSR_EC(ctx.hsr);
	ctx.regs = guest_regs->usr;

	/*
	 * On some implementations, instructions that fail their condition check
	 * can trap.
	 */
	if (arch_failed_condition(&ctx)) {
		arch_skip_instruction(&ctx);
		return;
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
}

static void arch_dump_exit(struct registers *regs, const char *reason)
{
	unsigned long pc;
	unsigned int n;

	arm_read_banked_reg(ELR_hyp, pc);
	panic_printk("Unhandled HYP %s exit at 0x%lx\n", reason, pc);
	for (n = 0; n < NUM_USR_REGS; n++)
		panic_printk("r%d:%s 0x%08lx%s", n, n < 10 ? " " : "",
			     regs->usr[n], n % 4 == 3 ? "\n" : "  ");
	panic_printk("\n");
}

static void arch_dump_abt(bool is_data)
{
	u32 hxfar;
	u32 hsr;

	arm_read_sysreg(HSR, hsr);
	if (is_data)
		arm_read_sysreg(HDFAR, hxfar);
	else
		arm_read_sysreg(HIFAR, hxfar);

	panic_printk("Physical address: 0x%08x HSR: 0x%08x\n", hxfar, hsr);
}

struct registers* arch_handle_exit(struct per_cpu *cpu_data,
				   struct registers *regs)
{
	cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_TOTAL]++;

	switch (regs->exit_reason) {
	case EXIT_REASON_IRQ:
		irqchip_handle_irq(cpu_data);
		break;
	case EXIT_REASON_TRAP:
		arch_handle_trap(regs);
		break;

	case EXIT_REASON_UNDEF:
		arch_dump_exit(regs, "undef");
		panic_stop();
	case EXIT_REASON_DABT:
		arch_dump_exit(regs, "data abort");
		arch_dump_abt(true);
		panic_stop();
	case EXIT_REASON_PABT:
		arch_dump_exit(regs, "prefetch abort");
		arch_dump_abt(false);
		panic_stop();
	case EXIT_REASON_HVC:
		arch_dump_exit(regs, "hvc");
		panic_stop();
	case EXIT_REASON_FIQ:
		arch_dump_exit(regs, "fiq");
		panic_stop();
	default:
		arch_dump_exit(regs, "unknown");
		panic_stop();
	}

	return regs;
}
