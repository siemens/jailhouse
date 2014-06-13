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

#define PSR_MODE_MASK	0xf
#define PSR_USR_MODE	0x0
#define PSR_FIQ_MODE	0x1
#define PSR_IRQ_MODE	0x2
#define PSR_SVC_MODE	0x3
#define PSR_MON_MODE	0x6
#define PSR_ABT_MODE	0x7
#define PSR_HYP_MODE	0xa
#define PSR_UND_MODE	0xb
#define PSR_SYS_MODE	0xf

#define PSR_32_BIT	(1 << 4)
#define PSR_T_BIT	(1 << 5)
#define PSR_F_BIT	(1 << 6)
#define PSR_I_BIT	(1 << 7)
#define PSR_A_BIT	(1 << 8)

#define MPIDR_CPUID_MASK	0x00ffffff

#ifndef __ASSEMBLY__

struct registers {
};

#define dmb(domain)	asm volatile("dmb " #domain "\n" ::: "memory")
#define dsb(domain)	asm volatile("dsb " #domain "\n" ::: "memory")
#define isb()		asm volatile("isb\n")

#define wfe()		asm volatile("wfe\n")
#define wfi()		asm volatile("wfi\n")
#define sev()		asm volatile("sev\n")

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
