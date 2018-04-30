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

#ifndef _JAILHOUSE_ASM_PROCESSOR_H
#define _JAILHOUSE_ASM_PROCESSOR_H

#include <jailhouse/types.h>
#include <jailhouse/utils.h>

#define EXIT_REASON_EL2_ABORT	0x0
#define EXIT_REASON_EL1_ABORT	0x1
#define EXIT_REASON_EL1_IRQ	0x2

#define NUM_USR_REGS		31

#ifndef __ASSEMBLY__

union registers {
	struct {
		unsigned long exit_reason;
		unsigned long usr[NUM_USR_REGS];
	};
};

#define ARM_PARKING_CODE		\
	0xd503207f, /* 1: wfi  */	\
	0x17ffffff, /*    b 1b */

#define dmb(domain)	asm volatile("dmb " #domain "\n" : : : "memory")
#define dsb(domain)	asm volatile("dsb " #domain "\n" : : : "memory")
#define isb()		asm volatile("isb\n")

static inline void cpu_relax(void)
{
	asm volatile("" : : : "memory");
}

static inline void memory_barrier(void)
{
	dmb(ish);
}

static inline void memory_load_barrier(void)
{
}

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PROCESSOR_H */
