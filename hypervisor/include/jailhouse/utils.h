/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Partly derived from Linux kernel code.
 */

/*
 * We need guards around ARRAY_SIZE as there is a duplicate definition in
 * jailhouse/cell-config.h due to header license incompatibility. Once
 * ARRAY_SIZE is replaced in cell-config.h, this guard can be removed.
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array)	(sizeof(array) / sizeof((array)[0]))
#endif

/* sizeof() for a structure/union field */
#define FIELD_SIZEOF(type, fld)	(sizeof(((type *)0)->fld))

/* create 64-bit mask with bytes 0 to size-1 set to 0xff */
#define BYTE_MASK(size)		(0xffffffffffffffffULL >> ((8 - (size)) * 8))

/* create 64-bit mask with all bits in [last:first] set */
#define BIT_MASK(last, first) \
	((0xffffffffffffffffULL >> (64 - ((last) + 1 - (first)))) << (first))

/* extract the field value at [last:first] from an input of up to 64 bits */
#define GET_FIELD(value, last, first) \
	(((value) & BIT_MASK((last), (first))) >> (first))

#define MAX(a, b)		((a) >= (b) ? (a) : (b))
#define MIN(a, b)		((a) <= (b) ? (a) : (b))
