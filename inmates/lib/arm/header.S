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
	.arm

	.section ".boot", "ax"
	.align 5
vectors:
	b	vector_reset
	b	vector_undef
	b	vector_svc
	b	vector_pabt
	b	vector_dabt
	b	vector_unused
	b	vector_irq
	b	vector_fiq

.macro vector, name
	.weak vector_\name
	vector_\name:
	b	.
.endm
	vector undef
	vector svc
	vector pabt
	vector dabt
	vector unused
	vector irq
	vector fiq

vector_reset:
	ldr	r0, =vectors
	mcr	p15, 0, r0, c12, c0, 0	@ VBAR

	ldr	sp, =stack_top

	b	inmate_main

	.ltorg
