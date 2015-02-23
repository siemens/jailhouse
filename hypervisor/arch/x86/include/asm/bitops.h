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
 * This file is based on linux/arch/x86/include/asm/bitops.h:
 *
 * Copyright 1992, Linus Torvalds.
 * Copyright (c) Linux kernel developers, 2013
 */

#ifndef _JAILHOUSE_ASM_BITOPS_H
#define _JAILHOUSE_ASM_BITOPS_H

#include <jailhouse/types.h>

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 1)
/* Technically wrong, but this avoids compilation errors on some gcc
   versions. */
#define BITOP_ADDR(x) "=m" (*(volatile long *) (x))
#else
#define BITOP_ADDR(x) "+m" (*(volatile long *) (x))
#endif

#define CONST_MASK_ADDR(nr, addr)	BITOP_ADDR((void *)(addr) + ((nr)>>3))
#define CONST_MASK(nr)			(1 << ((nr) & 7))

static inline __attribute__((always_inline)) void
clear_bit(int nr, volatile unsigned long *addr)
{
	if (__builtin_constant_p(nr)) {
		asm volatile("lock andb %1,%0"
			: CONST_MASK_ADDR(nr, addr)
			: "iq" ((u8)~CONST_MASK(nr)));
	} else {
		asm volatile("lock btr %1,%0"
			: BITOP_ADDR(addr)
			: "Ir" (nr));
	}
}

static inline __attribute__((always_inline)) void
set_bit(unsigned int nr, volatile unsigned long *addr)
{
	if (__builtin_constant_p(nr)) {
		asm volatile("lock orb %1,%0"
			: CONST_MASK_ADDR(nr, addr)
			: "iq" ((u8)CONST_MASK(nr))
			: "memory");
	} else {
		asm volatile("lock bts %1,%0"
			: BITOP_ADDR(addr) : "Ir" (nr) : "memory");
	}
}

static inline __attribute__((always_inline)) int
constant_test_bit(unsigned int nr, const volatile unsigned long *addr)
{
	return ((1UL << (nr % BITS_PER_LONG)) &
		(addr[nr / BITS_PER_LONG])) != 0;
}

static inline int variable_test_bit(int nr, volatile const unsigned long *addr)
{
	int oldbit;

	asm volatile("bt %2,%1\n\t"
		     "sbb %0,%0"
		     : "=r" (oldbit)
		     : "m" (*(unsigned long *)addr), "Ir" (nr));

	return oldbit;
}

#define test_bit(nr, addr)			\
	(__builtin_constant_p((nr))		\
	 ? constant_test_bit((nr), (addr))	\
	 : variable_test_bit((nr), (addr)))

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	int oldbit;

	asm volatile("lock bts %2,%1\n\t"
		     "sbb %0,%0" : "=r" (oldbit), BITOP_ADDR(addr)
		     : "Ir" (nr) : "memory");

	return oldbit;
}

static inline unsigned long ffzl(unsigned long word)
{
	asm("rep; bsf %1,%0"
		: "=r" (word)
		: "r" (~word));
	return word;
}

static inline unsigned long ffsl(unsigned long word)
{
	asm("rep; bsf %1,%0"
		: "=r" (word)
		: "rm" (word));
	return word;
}

#endif /* !_JAILHOUSE_ASM_BITOPS_H */
