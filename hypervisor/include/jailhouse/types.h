/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_TYPES_H
#define _JAILHOUSE_TYPES_H

#include <asm/types.h>

#define NULL				((void *)0)

#ifndef __ASSEMBLY__

typedef enum { true = 1, false = 0 } bool;

/** Describes a CPU set. */
struct cpu_set {
	/** Maximum CPU ID expressible with this set. */
	unsigned long max_cpu_id;
	/** Bitmap of CPUs in the set.
	 * @note Typically, the bitmap is extended by embedding this structure
	 * into a larger buffer. */
	unsigned long bitmap[1];
};

typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

typedef s8 __s8;
typedef u8 __u8;

typedef s16 __s16;
typedef u16 __u16;

typedef s32 __s32;
typedef u32 __u32;

typedef s64 __s64;
typedef u64 __u64;

#if BITS_PER_LONG == 64
typedef unsigned long size_t;
#else
typedef unsigned int size_t;
#endif

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_JAILHOUSE_TYPES_H */
