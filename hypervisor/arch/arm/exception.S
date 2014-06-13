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

#include <asm/head.h>

	.text
	.globl hyp_vectors
	.align 5
hyp_vectors:
	b	.
	b	.
	b	.
	b	.
	b	.
	b	.
	b	.
	b	.
