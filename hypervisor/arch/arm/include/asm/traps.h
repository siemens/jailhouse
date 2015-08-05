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

#ifndef _JAILHOUSE_ASM_TRAPS_H
#define _JAILHOUSE_ASM_TRAPS_H

#include <jailhouse/printk.h>
#include <jailhouse/types.h>
#include <asm/head.h>
#include <asm/percpu.h>

#ifndef __ASSEMBLY__

enum trap_return {
	TRAP_HANDLED		= 1,
	TRAP_UNHANDLED		= 0,
	TRAP_FORBIDDEN		= -1,
};

struct trap_context {
	unsigned long *regs;
	u32 esr;
	u32 cpsr;
	u32 pc;
};

typedef int (*trap_handler)(struct trap_context *ctx);

#define arm_read_banked_reg(reg, val)					\
	asm volatile ("mrs %0, " #reg "\n" : "=r" (val))

#define arm_write_banked_reg(reg, val)					\
	asm volatile ("msr " #reg ", %0\n" : : "r" (val))

#define _access_banked(reg, val, is_read)				\
	do {								\
		if (is_read)						\
			arm_write_banked_reg(reg, val);			\
		else							\
			arm_read_banked_reg(reg, val);			\
	} while (0)

#define access_banked_reg(mode, reg, val, is_read)			\
	do {								\
		switch (reg) {						\
		case 13:						\
			_access_banked(SP_##mode, *val, is_read);	\
			break;						\
		case 14:						\
			_access_banked(LR_##mode, *val, is_read);	\
			break;						\
		default:						\
			printk("ERROR: access r%d in "#mode"\n", reg);	\
		}							\
	} while (0)

static inline void access_fiq_reg(u8 reg, unsigned long *val, bool is_read)
{
	switch (reg) {
	case 8:  _access_banked(r8_fiq,  *val, is_read); break;
	case 9:  _access_banked(r9_fiq,  *val, is_read); break;
	case 10: _access_banked(r10_fiq, *val, is_read); break;
	case 11: _access_banked(r11_fiq, *val, is_read); break;
	case 12: _access_banked(r12_fiq, *val, is_read); break;
	default:
		 /* Use existing error reporting */
		 access_banked_reg(fiq, reg, val, is_read);
	}
}

static inline void access_usr_reg(struct trap_context *ctx, u8 reg,
				  unsigned long *val, bool is_read)
{
	if (is_read)
		*val = ctx->regs[reg];
	else
		ctx->regs[reg] = *val;
}

void access_cell_reg(struct trap_context *ctx, u8 reg, unsigned long *val,
		     bool is_read);
void arch_skip_instruction(struct trap_context *ctx);

int arch_handle_dabt(struct trap_context *ctx);

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_TRAPS_H */
