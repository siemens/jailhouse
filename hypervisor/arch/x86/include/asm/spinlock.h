/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * This file is based on linux/arch/x86/include/asm/spinlock.h:
 *
 * Copyright (c) Linux kernel developers, 2014
 */

#ifndef _JAILHOUSE_ASM_SPINLOCK_H
#define _JAILHOUSE_ASM_SPINLOCK_H

#include <asm/processor.h>

typedef struct {
	u16 owner, next;
} spinlock_t;

#define DEFINE_SPINLOCK(name)	spinlock_t (name)

static inline void spin_lock(spinlock_t *lock)
{
	register spinlock_t inc = { .next = 1 };

	asm volatile("lock xaddl %0, %1"
		: "+r" (inc), "+m" (*lock)
		: : "memory", "cc");

	if (inc.owner != inc.next)
		while (lock->owner != inc.next)
			cpu_relax();

	asm volatile("" : : : "memory");
}

static inline void spin_unlock(spinlock_t *lock)
{
	asm volatile("addw %1, %0"
		: "+m" (lock->owner)
		: "ri" (1)
		: "memory", "cc");
}

#endif /* !_JAILHOUSE_ASM_SPINLOCK_H */
