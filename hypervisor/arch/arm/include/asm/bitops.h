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

#ifndef _JAILHOUSE_ASM_BITOPS_H
#define _JAILHOUSE_ASM_BITOPS_H

#include <asm/types.h>

static inline __attribute__((always_inline)) void
clear_bit(int nr, volatile unsigned long *addr)
{
}

static inline __attribute__((always_inline)) void
set_bit(unsigned int nr, volatile unsigned long *addr)
{
}

static inline __attribute__((always_inline)) int
constant_test_bit(unsigned int nr, const volatile unsigned long *addr)
{
	return ((1UL << (nr % BITS_PER_LONG)) &
		(addr[nr / BITS_PER_LONG])) != 0;
}

static inline int variable_test_bit(int nr, volatile const unsigned long *addr)
{
	return 0;
}

#define test_bit(nr, addr)			\
	(__builtin_constant_p((nr))		\
	 ? constant_test_bit((nr), (addr))	\
	 : variable_test_bit((nr), (addr)))

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	return 0;
}

static inline unsigned long ffz(unsigned long word)
{
	return 0;
}

#endif /* !_JAILHOUSE_ASM_BITOPS_H */
