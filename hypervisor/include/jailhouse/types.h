/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013, 2014
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

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_JAILHOUSE_TYPES_H */
