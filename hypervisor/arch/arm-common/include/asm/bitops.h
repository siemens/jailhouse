/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2020
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
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
