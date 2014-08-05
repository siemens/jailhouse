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

#include <asm/head.h>
#include <asm/percpu.h>

/* Entry point for Linux loader module on JAILHOUSE_ENABLE */
	.text
	.globl arch_entry
arch_entry:
	mvn %r0,#~-38
	bx %lr


/* Fix up Global Offset Table with absolute hypervisor address */
	.globl got_init
got_init:
	bx %lr
