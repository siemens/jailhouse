/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_DRIVER_MAIN_H
#define _JAILHOUSE_DRIVER_MAIN_H

#include <linux/mutex.h>

#include "cell.h"

#ifdef CONFIG_X86
#define JAILHOUSE_ARCHITECTURE	JAILHOUSE_X86
#elif defined(CONFIG_ARM)
#define JAILHOUSE_ARCHITECTURE	JAILHOUSE_ARM
#elif defined(CONFIG_ARM64)
#define JAILHOUSE_ARCHITECTURE	JAILHOUSE_ARM64
#endif

extern struct mutex jailhouse_lock;
extern bool jailhouse_enabled;
extern void *hypervisor_mem;

void *jailhouse_ioremap(phys_addr_t phys, unsigned long virt,
			unsigned long size);
int jailhouse_console_dump_delta(char *dst, unsigned int head,
				 unsigned int *miss);

#endif /* !_JAILHOUSE_DRIVER_MAIN_H */
