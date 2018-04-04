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

#include <asm/asm-defines.h>
#include <asm/processor.h>

	.arch_extension virt

/* Entry point for Linux loader module on JAILHOUSE_ENABLE */
	.text
	.globl arch_entry
arch_entry:
	/* r0: cpuid */
	push	{r0 - r12}

	ldr	r1, =__page_pool
	mov	r4, #1
	lsl	r4, #PERCPU_SIZE_SHIFT_ASM
	/*
	 * percpu data = pool + cpuid * shift
	 * TODO: handle aff1 and aff2
	 */
	mla	r1, r4, r0, r1
	add	r4, r1, #PERCPU_LINUX_SP

	/*
	 * Save SP, LR, CPSR
	 * r4 is used so that they can be easily retrieved on failure.
	 */
	str	sp, [r4], #4
	str	lr, [r4], #4
	mrs	r3, cpsr
	str	r3, [r4]

	mov	sp, r1
	add	sp, #PERCPU_STACK_END
	/*
	 * Keep some space for a struct registers, in case setup fails and needs
	 * to return to the driver through the arch_shutdown_self path.
	 */
	sub	sp, #((NUM_USR_REGS + 1) * 4)
	/* Call entry(cpuid, struct per_cpu*) */
	bl	entry

	/*
	 * entry only returns here when there is an error before setting up EL2
	 */
	ldr	r3, [r4], #-4
	msr	spsr, r3
	ldr	lr, [r4], #-4
	ldr	sp, [r4]

	/* Keep the return value in r0 */
	pop	{r1}
	pop	{r1 - r12}
	subs	pc, lr, #0


	.globl bootstrap_vectors
	.align 5
bootstrap_vectors:
	b	.
	b	.
	b	.
	b	.
	b	.
	b	setup_el2
	b	.
	b	.

setup_el2:
	/*
	 * Load the physical values of lr and sp, and continue execution at EL2.
	 */
	mov	lr, r0
	mov	sp, r1

	bx	lr


	.globl hyp_vectors
	.align 5
hyp_vectors:
	b	.
	b	hyp_undef
	b	hyp_hvc
	b	hyp_pabt
	b	hyp_dabt
	b	hyp_trap
	b	hyp_irq
	b	hyp_fiq

.macro handle_vmexit exit_reason
	/* Fill the struct registers. Should comply with NUM_USR_REGS */
	push	{r0-r12, lr}
	mov	r0, #\exit_reason
	b	vmexit_common
.endm

hyp_undef:
	handle_vmexit EXIT_REASON_UNDEF
hyp_hvc:
	handle_vmexit EXIT_REASON_HVC
hyp_pabt:
	handle_vmexit EXIT_REASON_PABT
hyp_dabt:
	handle_vmexit EXIT_REASON_DABT

hyp_irq:
	handle_vmexit EXIT_REASON_IRQ
hyp_fiq:
	handle_vmexit EXIT_REASON_FIQ
hyp_trap:
	handle_vmexit EXIT_REASON_TRAP

vmexit_common:
	push	{r0}

	mov	r0, sp
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
