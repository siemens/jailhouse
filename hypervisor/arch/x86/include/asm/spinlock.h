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

#include <asm/bitops.h>
#include <asm/processor.h>

typedef struct {
	unsigned long state;
} spinlock_t;

#define DEFINE_SPINLOCK(name)	spinlock_t (name)

static inline void spin_lock(spinlock_t *lock)
{
	while (test_and_set_bit(0, &lock->state))
		cpu_relax();
}

static inline void spin_unlock(spinlock_t *lock)
{
	asm volatile("": : :"memory");
	clear_bit(0, &lock->state);
}
