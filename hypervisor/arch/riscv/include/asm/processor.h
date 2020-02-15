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

#ifndef _JAILHOUSE_ASM_PROCESSOR_H
#define _JAILHOUSE_ASM_PROCESSOR_H

#include <jailhouse/types.h>

union registers {
};

static inline void cpu_relax(void)
{
}

static inline void memory_barrier(void)
{
}

static inline void memory_load_barrier(void)
{
}

#endif /* !_JAILHOUSE_ASM_PROCESSOR_H */
