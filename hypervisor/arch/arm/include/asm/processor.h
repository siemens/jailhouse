/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_PROCESSOR_H
#define _JAILHOUSE_ASM_PROCESSOR_H

#include <jailhouse/types.h>
#include <jailhouse/utils.h>
#include <asm/sysregs.h>

#define EXIT_REASON_UNDEF	0x1
#define EXIT_REASON_HVC		0x2
#define EXIT_REASON_PABT	0x3
#define EXIT_REASON_DABT	0x4
#define EXIT_REASON_TRAP	0x5
#define EXIT_REASON_IRQ		0x6
#define EXIT_REASON_FIQ		0x7

#define NUM_USR_REGS		14

#ifndef __ASSEMBLY__

union registers {
	struct {
		unsigned long exit_reason;
		/* r0 - r12 and lr. The other registers are banked. */
		unsigned long usr[NUM_USR_REGS];
	};
};

#define ARM_PARKING_CODE		\
	0xe320f003, /* 1: wfi  */	\
	0xeafffffd, /*    b 1b */

#define dmb(domain)	asm volatile("dmb " #domain ::: "memory")
#define dsb(domain)	asm volatile("dsb " #domain ::: "memory")
#define isb()		asm volatile("isb")

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

static inline bool is_el2(void)
{
	u32 psr;

	asm volatile ("mrs	%0, cpsr" : "=r" (psr));

	return (psr & PSR_MODE_MASK) == PSR_HYP_MODE;
}

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PROCESSOR_H */
