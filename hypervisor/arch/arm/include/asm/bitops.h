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

/* also include from arm-common */
#include_next <asm/bitops.h>

static inline int atomic_test_and_set_bit(int nr, volatile unsigned long *addr)
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
