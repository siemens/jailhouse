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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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

	b	c_entry

handle_irq:
	sub sp, sp, #(16 * 16)
	stp x0, x1, [sp, #(0 * 16)]
	stp x2, x3, [sp, #(1 * 16)]
	stp x4, x5, [sp, #(2 * 16)]
	stp x6, x7, [sp, #(3 * 16)]
	stp x8, x9, [sp, #(4 * 16)]
	stp x10, x11, [sp, #(5 * 16)]
	stp x12, x13, [sp, #(6 * 16)]
	stp x14, x15, [sp, #(7 * 16)]
	stp x16, x17, [sp, #(8 * 16)]
	stp x18, x19, [sp, #(9 * 16)]
	stp x20, x21, [sp, #(10 * 16)]
	stp x22, x23, [sp, #(11 * 16)]
	stp x24, x25, [sp, #(12 * 16)]
	stp x26, x27, [sp, #(13 * 16)]
	stp x28, x29, [sp, #(14 * 16)]
	str x30, [sp, #(15 * 16)]

	bl	vector_irq

	ldp x0, x1, [sp, #(0 * 16)]
	ldp x2, x3, [sp, #(1 * 16)]
	ldp x4, x5, [sp, #(2 * 16)]
	ldp x6, x7, [sp, #(3 * 16)]
	ldp x8, x9, [sp, #(4 * 16)]
	ldp x10, x11, [sp, #(5 * 16)]
	ldp x12, x13, [sp, #(6 * 16)]
	ldp x14, x15, [sp, #(7 * 16)]
	ldp x16, x17, [sp, #(8 * 16)]
	ldp x18, x19, [sp, #(9 * 16)]
	ldp x20, x21, [sp, #(10 * 16)]
	ldp x22, x23, [sp, #(11 * 16)]
	ldp x24, x25, [sp, #(12 * 16)]
	ldp x26, x27, [sp, #(13 * 16)]
	ldp x28, x29, [sp, #(14 * 16)]
	ldr x30, [sp, #(15 * 16)]
	add sp, sp, #(16 * 16)

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
