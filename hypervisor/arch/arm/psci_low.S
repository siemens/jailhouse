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
#include <asm/psci.h>

	.arch_extension sec
	.globl smc
	/*
	 * Since we trap all SMC instructions, it may be useful to forward them
	 * when it isn't a PSCI call. The shutdown code will also have to issue
	 * a real PSCI_OFF call on secondary CPUs.
	 */
smc:
	smc	#0
	bx	lr

	.global _psci_cpu_off
	/* r0: struct psci_mbox* */
_psci_cpu_off:
	ldr	r2, =PSCI_INVALID_ADDRESS
	/* Clear mbox */
	str	r2, [r0]
	/*
	 * No reordering against the ldr below for the PEs in our domain, so no
	 * need for a barrier. Other CPUs will wait for an invalid address
	 * before issuing a CPU_ON.
	 */

	/* Wait for a CPU_ON call that updates the mbox */
1:	wfe
	ldr	r1, [r0]
	cmp	r1, r2
	beq	1b

	/* Jump to the requested entry, with a parameter */
	ldr	r0, [r0, #4]
	bx	r1

	.global _psci_cpu_on
	/* r0: struct psci_mbox*, r1: entry, r2: context */
_psci_cpu_on:
	push	{r4, r5, lr}
	/* strd needs to start with an even register */
	mov	r3, r2
	mov	r2, r1
	ldr	r1, =PSCI_INVALID_ADDRESS

	ldrexd	r4, r5, [r0]
	cmp	r4, r1
	bne	store_failed
	strexd	r1, r2, r3, [r0]
	/* r1 contains the ex store flag */
	cmp	r1, #0
	bne	store_failed

	/*
	 * Ensure that the stopped CPU can read the new address when receiving
	 * the event.
	 */
	dsb	ish
	sev
	mov	r0, #0
	pop	{r4, r5, pc}

store_failed:
	clrex
	mov	r0, #PSCI_ALREADY_ON
	pop	{r4, r5, pc}

	.global _psci_suspend_return
_psci_suspend_return:
	bx	lr
