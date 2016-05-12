/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2016 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) 2016 Siemens AG
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Implementation derived from Linux source files:
 *   - arch/arm64/mm/cache.S
 *   - arch/arm64/mm/proc-macros.S
 */

#include <asm/asm-defines.h>

/*
 * dcache_line_size - get the minimum D-cache line size from the CTR register.
 */
	.macro	dcache_line_size, reg, tmp
	mrs	\tmp, ctr_el0			// read CTR
	ubfm	\tmp, \tmp, #16, #19		// cache line size encoding
	mov	\reg, #4			// bytes per word
	lsl	\reg, \reg, \tmp		// actual cache line size
	.endm

/*
 *	arm_dcaches_flush(addr, size, flush)
 *
 *	Ensure that the data held in the page addr is written back to the
 *	page in question.
 *
 *	- addr    - address
 *	- size    - size in question
 *	- flush   - type of flush (see enum dcache_flush)
 */
	.global arm_dcaches_flush
arm_dcaches_flush:
	dcache_line_size x3, x4
	add	x1, x0, x1
	sub	x4, x3, #1
	bic	x0, x0, x4

1:	cmp	x2, #DCACHE_CLEAN_ASM
	b.ne	2f
	dc	cvac, x0
	b	4f

2:	cmp	x2, #DCACHE_INVALIDATE_ASM
	b.ne	3f
	dc	ivac, x0
	b	4f

3:	dc	civac, x0			// DCACHE_CLEAN_AND_INVALIDATE

4:	add	x0, x0, x3
	cmp	x0, x1
	b.lo	1b

	dsb	sy
	ret
