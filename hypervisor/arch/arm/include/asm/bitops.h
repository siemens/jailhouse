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

static inline __attribute__((always_inline)) int
test_bit(unsigned int nr, const volatile unsigned long *addr)
{
	return ((1UL << (nr % BITS_PER_LONG)) &
		(addr[nr / BITS_PER_LONG])) != 0;
}

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long ret, val, test;

	/* word-align */
	addr = (unsigned long *)((u32)addr & ~0x3) + nr / BITS_PER_LONG;
	nr %= BITS_PER_LONG;

	/* Load the cacheline in exclusive state */
	asm volatile (
		".arch_extension mp\n\t"
		"pldw %0\n\t"
		: "+Qo" (*(volatile unsigned long *)addr));
	do {
		asm volatile (
			"ldrex	%1, %3\n\t"
			"ands	%2, %1, %4\n\t"
			"it	eq\n\t"
			"orreq	%1, %4\n\t"
			"strex	%0, %1, %3\n\t"
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
	asm volatile ("clz %0, %1" : "=r" (val) : "r" (word));
	return val;
}

/* Returns the position of the least significant 1, MSB=31, LSB=0*/
static inline unsigned long ffsl(unsigned long word)
{
	if (!word)
		return 0;
	asm volatile ("rbit %0, %0" : "+r" (word));
	return clz(word);
}

static inline unsigned long ffzl(unsigned long word)
{
	return ffsl(~word);
}
