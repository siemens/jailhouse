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

// this shouldn't be here
#include <asm/processor.h>

#define STACK_SIZE			PAGE_SIZE

#define ARCH_PUBLIC_PERCPU_FIELDS					\
	spinlock_t control_lock;					\
	;

#define ARCH_PERCPU_FIELDS						\
	;
