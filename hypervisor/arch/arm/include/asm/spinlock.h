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
 *
 * Copied from arch/arm/include/asm/spinlock.h in Linux
 */
#ifndef _JAILHOUSE_ASM_SPINLOCK_H
#define _JAILHOUSE_ASM_SPINLOCK_H

#include <asm/processor.h>

#ifndef __ASSEMBLY__

#define DEFINE_SPINLOCK(name)	spinlock_t (name)
#define TICKET_SHIFT		16

typedef struct {
	union {
		u32 slock;
		struct __raw_tickets {
			u16 owner;
			u16 next;
		} tickets;
	};
} spinlock_t;

static inline void spin_lock(spinlock_t *lock)
{
	unsigned long tmp;
	u32 newval;
	spinlock_t lockval;

	/* Take the lock by updating the high part atomically */
	asm volatile (
		".arch_extension mp\n\t"
		"pldw	[%3]\n\t"
		"1:\n\t"
		"ldrex	%0, [%3]\n\t"
		"add	%1, %0, %4\n\t"
		"strex	%2, %1, [%3]\n\t"
		"teq	%2, #0\n\t"
		"bne	1b\n\t"
		: "=&r" (lockval), "=&r" (newval), "=&r" (tmp)
		: "r" (&lock->slock), "I" (1 << TICKET_SHIFT)
		: "cc");

	while (lockval.tickets.next != lockval.tickets.owner)
		asm volatile (
			"wfe\n\t"
			"ldrh	%0, [%1]\n\t"
			: "=r" (lockval.tickets.owner)
			: "r" (&lock->tickets.owner));

	/* Ensure we have the lock before doing any more memory ops */
	dmb(ish);
}

static inline void spin_unlock(spinlock_t *lock)
{
	/* Ensure all memory ops are finished before releasing the lock */
	dmb(ish);

	/* No need for an exclusive, since only one CPU can unlock at a time. */
	lock->tickets.owner++;

	/* Ensure the spinlock is updated before notifying other CPUs */
	dsb(ishst);
	sev();
}

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_SPINLOCK_H */
