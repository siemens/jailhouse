/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Claudio Fontana <claudio.fontana@huawei.com>
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

/* also include from arm-common */
#include_next <asm/bitops.h>

static inline int atomic_test_and_set_bit(int nr, volatile unsigned long *addr)
{
	u32 ret;
	u64 test, tmp;

	/* word-align */
	addr = (unsigned long *)((u64)addr & ~0x7) + nr / BITS_PER_LONG;
	nr %= BITS_PER_LONG;


	/* AARCH64_TODO: using Inner Shareable DMB at the moment,
	 * revisit when we will deal with shareability domains */

	do {
		asm volatile (
			"ldxr	%3, %2\n\t"
			"ands	%1, %3, %4\n\t"
			"b.ne	1f\n\t"
			"orr	%3, %3, %4\n\t"
			"1:\n\t"
			"stxr	%w0, %3, %2\n\t"
			"dmb    ish\n\t"
			: "=&r" (ret), "=&r" (test),
			  "+Q" (*(volatile unsigned long *)addr),
			  "=&r" (tmp)
			: "r" (1ul << nr));
	} while (ret);
	return !!(test);
}
