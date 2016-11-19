/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_TRAPS_H
#define _JAILHOUSE_ASM_TRAPS_H

#include <jailhouse/types.h>

enum trap_return {
	TRAP_HANDLED		= 1,
	TRAP_UNHANDLED		= 0,
	TRAP_FORBIDDEN		= -1,
};

struct trap_context {
	unsigned long *regs;
	u64 esr;
	u64 spsr;
	u64 sp;
};

typedef int (*trap_handler)(struct trap_context *ctx);

void arch_skip_instruction(struct trap_context *ctx);

int arch_handle_dabt(struct trap_context *ctx);

#endif /* !_JAILHOUSE_ASM_TRAPS_H */
