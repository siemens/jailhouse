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
	/* r0: cpuid */
	push	{r0 - r12}

	ldr	r1, =__page_pool
	mov	r2, #1
	lsl	r2, #PERCPU_SIZE_SHIFT
	/*
	 * percpu data = pool + cpuid * shift
	 * TODO: handle aff1 and aff2
	 */
	mla	r1, r2, r0, r1
	add	r2, r1, #PERCPU_LINUX_SP

	/* Save SP, LR, CPSR */
	str	sp, [r2], #4
	str	lr, [r2], #4
	mrs	r3, cpsr
	str	r3, [r2]

	mov	sp, r1
	add	sp, #PERCPU_STACK_END
	/* Call entry(cpuid, struct per_cpu*) */
	b	entry
