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
#include <asm/sysregs.h>

	.text
	.globl hyp_vectors
	.align 5
hyp_vectors:
	b	.
	b	.
	b	.
	b	.
	b	.
	b	hyp_trap
	b	.
	b	.

hyp_trap:
	/* Fill the struct registers. Should comply with NUM_USR_REGS */
	push	{r0-r12, lr}

	arm_read_sysreg(TPIDR_EL2, r0)
	mov	r1, sp
	bl	arch_handle_trap

	/* Restore usr regs */
	pop	{r0-r12, lr}
	eret
