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
	u32 hsr;
};

typedef int (*trap_handler)(struct trap_context *ctx);

#define access_banked_reg(mode, reg, val, is_read)			\
	do {								\
		switch (reg) {						\
		case 13:						\
			arm_rw_banked_reg(SP_##mode, *val, is_read);	\
			break;						\
		case 14:						\
			arm_rw_banked_reg(LR_##mode, *val, is_read);	\
			break;						\
		default:						\
			printk("ERROR: access r%d in "#mode"\n", reg);	\
		}							\
	} while (0)

void access_cell_reg(struct trap_context *ctx, u8 reg, unsigned long *val,
		     bool is_read);
void arch_skip_instruction(struct trap_context *ctx);

int arch_handle_dabt(struct trap_context *ctx);

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_TRAPS_H */
