/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

.macro	ventry	label
	.align	7
	b	\label
.endm

	.section ".boot", "ax"
	.globl __reset_entry
__reset_entry:
	ldr	x0, =vectors
	msr	vbar_el1, x0

	ldr	x0, =stack_top
	mov	sp, x0

	mov	x0, #(3 << 20)
	msr	cpacr_el1, x0

	msr	daif, xzr

	isb

	b	inmate_main

handle_irq:
	bl	vector_irq
	eret

.weak vector_irq
	b	.

	.globl vectors
	.align 11
vectors:
	ventry	.
	ventry	.
	ventry	.
	ventry	.

	ventry	.
	ventry	handle_irq
	ventry	.
	ventry	.

	ventry	.
	ventry	handle_irq
	ventry	.
	ventry	.

	ventry	.
	ventry	.
	ventry	.
	ventry	.

	.ltorg
