/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2020
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_BITOPS_H
#define _JAILHOUSE_ASM_BITOPS_H

static inline __attribute__((always_inline)) int
test_bit(unsigned int nr, const volatile unsigned long *addr)
{
	return 0;
}

static inline int atomic_test_and_set_bit(int nr, volatile unsigned long *addr)
{
	return 0;
}

static inline unsigned long ffzl(unsigned long word)
{
	return 0;
}

static inline unsigned long ffsl(unsigned long word)
{
	return 0;
}

#endif /* !_JAILHOUSE_ASM_BITOPS_H */
