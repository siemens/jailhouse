/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2020
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define dmb(domain)	asm volatile("dmb " #domain ::: "memory")
#define dsb(domain)	asm volatile("dsb " #domain ::: "memory")
#define isb()		asm volatile("isb")

#ifndef __ASSEMBLY__

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
	dmb(ish);
}

#endif /* !__ASSEMBLY__ */
