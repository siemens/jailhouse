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

#include <asm/control.h>
#include <asm/sysregs.h>

/*
 * Clean the whole data cache
 * Taken from the ARM ARM example code (B2.2.7)
 */
	.global arm_dcaches_clean_by_sw
arm_dcaches_clean_by_sw:
	push	{r0-r10}

	dsb
	arm_read_sysreg(CLIDR_EL1, r0)
	ands	r3, r0, #0x07000000
	lsr	r3, #23			@ Extract level of coherency
	beq	finish

	mov	r9, #0			@ Cache level - 1
	@ Loop caches
loop_caches:
	add	r2, r9, r9, lsr #1
	lsr	r1, r0, r2		@ Extract current level type
	and	r1, r1, #7
	cmp	r1, #2
	blt	next_cache		@ No cache or instruction only

	arm_write_sysreg(CSSELR_EL1, r9)
	isb				@ sync selector change
	arm_read_sysreg(CSSIDR_EL1, r1)

	and	r2, r1, #7		@ extract log2(line size - 4)
	add	r2, #4
	ldr	r4, =0x3ff
	ands	r4, r4, r1, lsr #3
	clz	r5, r4			@ Max way size
	mov	r8, r5			@ Working copy of the way size

loop_sets:
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13	@ Max number of index size
loop_ways:
	orr	r10, r9, r8, lsl r5	@ Factor in the way and cache numbers
	orr	r10, r10, r7, lsl r2	@ Factor in the index number

	arm_write_sysreg(DCCSW, r10)

	subs	r7, r7, #1		@ decrement index
	bge	loop_ways
	subs	r8, r8, #1		@ decrement way
	bge	loop_sets

next_cache:
	add	r9, r9, #2		@ increment cache number
	cmp	r3, r9
	bgt	loop_caches
	dsb

finish:	isb
	pop	{r0-r10}
	bx	lr
