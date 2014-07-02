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

#include <jailhouse/types.h>

#ifndef __ASSEMBLY__

#define BITOPT_ALIGN(bits, addr)				\
	do {							\
		(addr) = (unsigned long *)((u32)(addr) & ~0x3)	\
			+ (bits) / BITS_PER_LONG;		\
		(bits) %= BITS_PER_LONG;			\
	} while (0)

/* Load the cacheline in exclusive state */
#define PRELOAD(addr)						\
	asm volatile (".arch_extension mp\n"			\
		      "pldw %0\n"				\
		      : "+Qo" (*(volatile unsigned long *)addr));

static inline __attribute__((always_inline)) void
clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long ret, val;

	BITOPT_ALIGN(nr, addr);

	PRELOAD(addr);
	do {
		asm volatile (
		"ldrex	%1, %2\n"
		"bic	%1, %3\n"
		"strex	%0, %1, %2\n"
		: "=r" (ret), "=r" (val),
		/* Declare the clobbering of this address to the compiler */
		  "+Qo" (*(volatile unsigned long *)addr)
		:  "r" (1 << nr));
	} while (ret);
}

static inline __attribute__((always_inline)) void
set_bit(unsigned int nr, volatile unsigned long *addr)
{
	unsigned long ret, val;

	BITOPT_ALIGN(nr, addr);

	PRELOAD(addr);
	do {
		asm volatile (
		"ldrex	%1, %2\n"
		"orr	%1, %3\n"
		"strex	%0, %1, %2\n"
		: "=r" (ret), "=r" (val),
		  "+Qo" (*(volatile unsigned long *)addr)
		: "r" (1 << nr));
	} while (ret);
}

static inline __attribute__((always_inline)) int
test_bit(unsigned int nr, const volatile unsigned long *addr)
{
	return ((1UL << (nr % BITS_PER_LONG)) &
		(addr[nr / BITS_PER_LONG])) != 0;
}

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long ret, val, test;

	BITOPT_ALIGN(nr, addr);

	PRELOAD(addr);
	do {
		asm volatile (
		"ldrex	%1, %3\n"
		"ands	%2, %1, %4\n"
		"it	eq\n"
		"orreq	%1, %4\n"
		"strex	%0, %1, %3\n"
		: "=r" (ret), "=r" (val), "=r" (test),
		  "+Qo" (*(volatile unsigned long *)addr)
		: "r" (1 << nr));
	} while (ret);

	return !!(test);
}


/* Count leading zeroes */
static inline unsigned long clz(unsigned long word)
{
	unsigned long val;
	asm volatile ("clz %0, %1\n" : "=r" (val) : "r" (word));
	return val;
}

/* Returns the position of the least significant 1, MSB=31, LSB=0*/
static inline unsigned long ffsl(unsigned long word)
{
	if (!word)
		return 0;
	asm volatile ("rbit %0, %0\n" : "+r" (word));
	return clz(word);
}

static inline unsigned long ffzl(unsigned long word)
{
	return ffsl(~word);
}

/* Extend the value of 'size' bits to a signed long */
static inline unsigned long sign_extend(unsigned long val, unsigned int size)
{
	unsigned long mask;

	if (size >= sizeof(unsigned long) * 8)
		return val;

	mask = 1U << (size - 1);
	return (val ^ mask) - mask;
}

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_BITOPS_H */
