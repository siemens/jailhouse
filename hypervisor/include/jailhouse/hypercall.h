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

#include <asm/jailhouse.h>

#define JAILHOUSE_HC_DISABLE		0
#define JAILHOUSE_HC_CELL_CREATE	1
#define JAILHOUSE_HC_CELL_DESTROY	2
