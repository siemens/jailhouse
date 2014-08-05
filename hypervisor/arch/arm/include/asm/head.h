/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_HEAD_H
#define _JAILHOUSE_ASM_HEAD_H_

#ifdef __ASSEMBLY__
	.arch_extension virt
	.arm
	.syntax unified
#else
	asm(".arch_extension virt\n");
#endif

#endif /* !_JAILHOUSE_ASM_HEAD_H */
