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

#include <jailhouse/processor.h>

struct trap_context {
	unsigned long *regs;
	u64 elr;
	u64 esr;
	u64 spsr;
	u64 sp;
};

void arch_handle_trap(union registers *guest_regs);
void arch_el2_abt(union registers *regs);

/* now include from arm-common */
#include_next <asm/traps.h>

#endif /* !_JAILHOUSE_ASM_TRAPS_H */
