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

#ifndef _JAILHOUSE_BITOPS_H
#define _JAILHOUSE_BITOPS_H

#include <jailhouse/types.h>
#include <asm/bitops.h>

static inline __attribute__((always_inline)) void
clear_bit(unsigned int nr, volatile unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] &= ~(1UL << (nr % BITS_PER_LONG));
}

static inline __attribute__((always_inline)) void
set_bit(unsigned int nr, volatile unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] |= 1UL << (nr % BITS_PER_LONG);
}

#endif /* !_JAILHOUSE_BITOPS_H */
