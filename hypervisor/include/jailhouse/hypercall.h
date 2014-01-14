/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_HYPERCALL_H
#define _JAILHOUSE_HYPERCALL_H

#include <asm/jailhouse_hypercall.h>

#define JAILHOUSE_HC_DISABLE		0
#define JAILHOUSE_HC_CELL_CREATE	1
#define JAILHOUSE_HC_CELL_DESTROY	2

struct jailhouse_comm_region {
};

#endif /* !_JAILHOUSE_HYPERCALL_H */
