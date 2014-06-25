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
#include <asm/processor.h>
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
	b	hyp_irq
	b	.

.macro handle_vmexit exit_reason
	/* Fill the struct registers. Should comply with NUM_USR_REGS */
	push	{r0-r12, lr}
	mov	r0, #\exit_reason
	b	vmexit_common
.endm

hyp_irq:
	handle_vmexit EXIT_REASON_IRQ
hyp_trap:
	handle_vmexit EXIT_REASON_TRAP

vmexit_common:
	push	{r0}

	arm_read_sysreg(TPIDR_EL2, r0)
	mov	r1, sp
	bl	arch_handle_exit

	/*
	 * Because the hypervisor may call vmreturn to reset the stack,
	 * arch_handle_exit has to return with the guest registers in r0
	 */
.globl vmreturn
vmreturn:
	mov	sp, r0
	add	sp, #4

	/* Restore usr regs */
	pop	{r0-r12, lr}
	eret
