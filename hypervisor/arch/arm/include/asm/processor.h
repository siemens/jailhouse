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
