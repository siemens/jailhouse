/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Partly derived from Linux kernel code.
 */

#define ARRAY_SIZE(array)	(sizeof(array) / sizeof((array)[0]))

/* sizeof() for a structure/union field */
#define FIELD_SIZEOF(type, fld)	(sizeof(((type *)0)->fld))

/* create 64-bit mask with bytes 0 to size-1 set to 0xff */
#define BYTE_MASK(size)		(0xffffffffffffffffULL >> ((8 - (size)) * 8))

/* create 64-bit mask with all bits in [last:first] set */
#define BIT_MASK(last, first) \
	((0xffffffffffffffffULL >> (64 - ((last) + 1 - (first)))) << (first))

#define MAX(a, b)		((a) >= (b) ? (a) : (b))
#define MIN(a, b)		((a) <= (b) ? (a) : (b))
