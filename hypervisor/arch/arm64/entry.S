/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015-2016 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) 2016 Siemens AG
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/paging.h>
#include <asm/asm-defines.h>
#include <asm/smccc.h>
#include <asm/jailhouse_hypercall.h>
#include <jailhouse/header.h>

#define LINUX_HVC_SET_VECTORS_LEGACY	1
#define LINUX_HVC_SET_VECTORS		0

	.data
vmexits_total:
	.quad CPU_STAT_VMEXITS_TOTAL

vmexits_smccc:
	.quad CPU_STAT_VMEXITS_SMCCC

/* x11 must contain the virt-to-phys offset */
.macro virt2phys, register
	add	\register, \register, x11
.endm

/* x11 must contain the virt-to-phys offset */
.macro phys2virt, register
	sub	\register, \register, x11
.endm

/* Entry point for Linux loader module on JAILHOUSE_ENABLE */
	.text
	.globl arch_entry
arch_entry:
	/*
	 * x0: cpuid
	 *
	 * We don't have access to our own address space yet, so we will
	 * abuse some caller saved registers to preserve across calls:
	 * x11: virtual-to-physical address offset
	 * x12: physical hypervisor address
	 * x13: virtual hypervisor address
	 * x14: physical UART address
	 * x15: virtual UART address
	 * x16: cpuid
	 * x17: caller lr
	 * x18: hyp-stub version
	 */
	mov	x16, x0
	mov	x17, x30

	adr     x0, hypervisor_header

	ldr	x18, [x0, #HEADER_HYP_STUB_VERSION]

	ldr	x15, =UART_BASE

	adrp	x1, __page_pool
	ldrh	w2, [x0, #HEADER_MAX_CPUS]
	mov	x3, #PERCPU_SIZE_ASM
	/*
	 * sysconfig = pool + max_cpus * percpu_size
	 */
	madd	x1, x2, x3, x1
	ldr	x14, [x1, #SYSCONFIG_DEBUG_CONSOLE_PHYS]

	ldr	x13, =JAILHOUSE_BASE

	ldr	x12, [x1, #SYSCONFIG_HYPERVISOR_PHYS]

	sub	x11, x12, x13

	/*
	 * Set jailhouse_header.debug_console_base to UART_BASE plus the offset
	 * into the UART's physical 2 MB page.
	 */
	and	x1, x14, #0x1fffff
	add	x15, x15, x1
	str	x15, [x0, #HEADER_DEBUG_CONSOLE_VIRT]

	/*
	 * When switching to EL2 using hvc #0, before the MMU is enabled, some
	 * data may still be kept in D-cache, such as the hypervisor core code.
	 * Flush it so that the CPU does not fetch wrong instructions.
	 */
	ldr	x1, [x0, #HEADER_CORE_SIZE]
	mov	x2, DCACHE_CLEAN_AND_INVALIDATE_ASM
	bl	arm_dcaches_flush

	/* install bootstrap_vectors */
	ldr	x1, =bootstrap_vectors
	virt2phys x1

	/* choose opcode */
	mov	x0, #LINUX_HVC_SET_VECTORS
	cmp	x18, #HYP_STUB_ABI_LEGACY
	b.ne	1f
	mov	x0, #LINUX_HVC_SET_VECTORS_LEGACY
1:
	hvc	#0

	hvc	#0	/* bootstrap vectors enter EL2 at el2_entry */
	b	.	/* we don't expect to return here */

	/* the bootstrap vector returns us here in physical addressing */
el2_entry:
	mrs	x1, esr_el2
	lsr	x1, x1, #26
	cmp	x1, #0x16
	b.ne	.		/* not hvc */

	/* init bootstrap page tables */
	bl	init_bootstrap_pt

	/* enable temporary mmu mapings for early initialization */
	adr	x0, bootstrap_pt_l0
	adr	x30, 1f		/* set lr manually to ensure... */
	phys2virt x30		/* ...that we return to a virtual address */
	b	enable_mmu_el2
1:
	/* install the final vectors */
	adr	x1, hyp_vectors
	msr	vbar_el2, x1

	mov	x0, x16		/* preserved cpuid, will be passed to entry */
	adrp	x1, __page_pool
	mov	x2, #PERCPU_SIZE_ASM
	/*
	 * percpu data = pool + cpuid * percpu_size
	 */
	madd	x1, x2, x0, x1

	/* set up the stack and push the root cell's callee saved registers */
	add	sp, x1, #PERCPU_STACK_END
	stp	x29, x17, [sp, #-16]!	/* note: our caller lr is in x17 */
	stp	x27, x28, [sp, #-16]!
	stp	x25, x26, [sp, #-16]!
	stp	x23, x24, [sp, #-16]!
	stp	x21, x22, [sp, #-16]!
	stp	x19, x20, [sp, #-16]!
	/*
	 * We pad the stack, so we can consistently access the guest
	 * registers from either the initialization, or the exception
	 * handling code paths. 19 caller saved registers plus the
	 * exit_reason, which we don't use on entry.
	 */
	sub	sp, sp, 20 * 8

	mrs	x29, id_aa64mmfr0_el1
	str	x29, [x1, #PERCPU_ID_AA64MMFR0]

	mov	x29, xzr	/* reset fp,lr */
	mov	x30, xzr

	/* Call entry(cpuid, struct per_cpu*). Should not return. */
	bl	entry
	b	.


/*
 * macros used by init_bootstrap_pt
 */

/* clobbers x8,x9 */
.macro	set_pte table, xidx, xval, flags
	add	x8, \xval, #(\flags)
	adr	x9, \table
	add	x9, x9, \xidx, lsl #3
	str	x8, [x9]
.endm

/* clobbers x8,x9 */
.macro	set_block table, index, addr, lvl
	and	x8, \addr, \
		#(((1 << ((\lvl + 1) * 9)) - 1) << (12 + (3 - \lvl) * 9))
	set_pte \table, \index, x8, PAGE_DEFAULT_FLAGS
.endm

/* clobbers x8,x9 */
.macro	set_block_dev table, index, addr, lvl
	and	x8, \addr, \
		#(((1 << ((\lvl + 1) * 9)) - 1) << (12 + (3 - \lvl) * 9))
	set_pte \table, \index, x8, (PAGE_DEFAULT_FLAGS|PAGE_FLAG_DEVICE)
.endm

/* clobbers x8,x9 */
.macro	set_table parent, index, child
	adr	x8, \child
	set_pte \parent, \index, x8, PTE_TABLE_FLAGS
.endm

.macro	get_index idx, addr, lvl
	ubfx	\idx, \addr, #(12 + (3 - \lvl) * 9), 9
.endm

init_bootstrap_pt:
	/*
	 * Initialize early page tables to bootstrap the
	 * initialization process. These tables will be replaced
	 * during hypervisor initialization.
	 *
	 * x0: physical address of trampoline page
	 * x12: physical address of hypervisor binary
	 * x13: virtual address of hypervisor binary
	 * x14: physical address of uart to map
	 * x15: virtual address of uart to map
	 *
	 * These are referenced statically for now.
	 *
	 * Clobbers x0-x4,x8,x9
	 */
	adrp	x0, __trampoline_start

	/* map the l1 table that includes the firmware and the uart */
	get_index x2, x13, 0
	set_table bootstrap_pt_l0, x2, bootstrap_pt_l1_hyp_uart

	/* map the l1 table that includes the trampoline */
	get_index x3, x0, 0
	set_table bootstrap_pt_l0, x3, bootstrap_pt_l1_trampoline

	/*  fill the l1 tables */
	get_index x2, x13, 1
	set_table bootstrap_pt_l1_hyp_uart, x2, bootstrap_pt_l2_hyp_uart
	get_index x4, x0, 1
	set_block bootstrap_pt_l1_trampoline, x4, x0, 1

	get_index x2, x13, 2
	set_block bootstrap_pt_l2_hyp_uart, x2, x12, 2
	get_index x3, x15, 2
	set_block_dev bootstrap_pt_l2_hyp_uart, x3, x14, 2

	adrp	x0, bootstrap_pt_l0
	mov	x1, PAGE_SIZE * 4
	mov	x2, DCACHE_INVALIDATE_ASM
	b	arm_dcaches_flush	/* will return to our caller */


.macro	ventry	label
	.align	7
	b	\label
.endm

	.align 11
bootstrap_vectors:
	ventry	.
	ventry	.
	ventry	.
	ventry	.

	ventry	.
	ventry	.
	ventry	.
	ventry	.

	ventry	el2_entry
	ventry	.
	ventry	.
	ventry	.

	ventry	.
	ventry	.
	ventry	.
	ventry	.

.macro handle_vmexit_early
	/* We need to save EL1 context, reserve some space on the stack */
	sub	sp, sp, #(16 * 16)
	/* And push [x1-x4] early, we need registers to work on */
	str	    x0, [sp, #(1 * 8)]
	stp	x1, x2, [sp, #(1 * 16)]
	stp	x3, x4, [sp, #(2 * 16)]

	/* [x0-x4] may now be clobbered. */

	/*
	 * increase vmexits_total on each exit. Using x3 and x4 will preserve
	 * x0, which still holds the guest's value on exit.
	 */
	ldr	x3, =vmexits_total
	ldr	x3, [x3]
	ldr	x4, [x3]
	add	x4, x4, #1
	str	x4, [x3]
.endm

.macro handle_vmexit_late handler
	/* Fill the rest of the union registers. Should comply with NUM_USR_REGS */
	stp	x5, x6, [sp, #(3 * 16)]
	stp	x7, x8, [sp, #(4 * 16)]
	stp	x9, x10, [sp, #(5 * 16)]
	stp	x11, x12, [sp, #(6 * 16)]
	stp	x13, x14, [sp, #(7 * 16)]
	stp	x15, x16, [sp, #(8 * 16)]
	stp	x17, x18, [sp, #(9 * 16)]
	stp	x19, x20, [sp, #(10 * 16)]
	stp	x21, x22, [sp, #(11 * 16)]
	stp	x23, x24, [sp, #(12 * 16)]
	stp	x25, x26, [sp, #(13 * 16)]
	stp	x27, x28, [sp, #(14 * 16)]
	stp	x29, x30, [sp, #(15 * 16)]

	mov	x29, xzr	/* reset fp,lr */
	mov	x30, xzr
	mov	x0, sp
	bl	\handler
	/* take the fast exit path, sp is already in place */
	b	__vmreturn
.endm

el1_trap:
	handle_vmexit_late arch_handle_trap

.macro handle_vmexit handler
	.align	7
	handle_vmexit_early
	handle_vmexit_late \handler
.endm

.macro handle_vmexit_hardened handler
	.align	7
	handle_vmexit_early

	/* Mitigate CVE 2017-5715 (aka Spectre v2) */
	mov	w0, #SMCCC_ARCH_WORKAROUND_1
	smc	#0

	handle_vmexit_late \handler
.endm

.macro handle_abort_fastpath
	.align	7
	handle_vmexit_early

	/* Save old x0, which might contain guest SMC's function ID */
	mov	x4, x0

	mov	w0, #SMCCC_ARCH_WORKAROUND_1
	smc	#0

	mrs	x0, esr_el2
	lsr	x0, x0, #ESR_EC_SHIFT
	cmp	x0, #ESR_EC_SMC64
	b.ne	el1_trap /* normal trap if !SMC64 */

	/* w4 holds the guest's function_id */
	eor	w0, w4, #SMCCC_ARCH_WORKAROUND_1
	cbnz	w0, el1_trap /* normal trap if !SMCCC_ARCH_WORKAROUND_1 */

	/* Here we land if the guest called SMCCC_ARCH_WORKAROUND_1 */

	/*
	 * Skip guest's instruction, it must have been 'smc #0' and must have
	 * had 4 bytes */
	mrs	x0, elr_el2
	add	x0, x0, #4
	msr	elr_el2, x0

	/* And don't forget to account the SMC exit */
	ldr	x0, =vmexits_smccc
	ldr	x0, [x0]
	ldr	x1, [x0]
	add	x1, x1, #1
	str	x1, [x0]

	/* beam me up, we only need to restore x4 and sp */
	ldr	x4, [sp, #(2 * 16 + 1 * 8)]
	add	sp, sp, #(16 * 16)
	eret
	/*
	 * Mitigate Straight-line Speculation.
	 * Guard against Speculating past an ERET instruction and
	 * potentially perform speculative accesses to memory before
	 * processing the exception return
	 */
	dsb nsh
	isb
.endm

/*
 * These are the default vectors. They are used on early startup and if no
 * Spectre v2 mitigation is available.
 */
	.align 11
hyp_vectors:
	ventry	.
	ventry	.
	ventry	.
	ventry	.

	handle_vmexit arch_el2_abt
	ventry	.
	ventry	.
	ventry	.

	handle_vmexit arch_handle_trap
	handle_vmexit irqchip_handle_irq
	ventry	.
	ventry	.

	handle_vmexit arch_handle_trap
	handle_vmexit irqchip_handle_irq
	ventry	.
	ventry	.


	.align 11
	.globl hyp_vectors_hardened
hyp_vectors_hardened:
	ventry	.
	ventry	.
	ventry	.
	ventry	.

	handle_vmexit arch_el2_abt /* no mitigation, we're doomed anyway... */
	ventry	.
	ventry	.
	ventry	.

	handle_abort_fastpath
	handle_vmexit_hardened irqchip_handle_irq
	ventry	.
	ventry	.

	handle_abort_fastpath
	handle_vmexit irqchip_handle_irq
	ventry	.
	ventry	.


	.pushsection	.trampoline, "ax"
	.globl enable_mmu_el2
enable_mmu_el2:
	/*
	 * x0: u64 ttbr0_el2
	 */

	/* setup the MMU for EL2 hypervisor mappings */
	ldr	x1, =DEFAULT_MAIR_EL2
	msr	mair_el2, x1

	/* AARCH64_TODO: ARM architecture supports CPU clusters which could be
	 * in separate inner shareable domains. At the same time: "The Inner
	 * Shareable domain is expected to be the set of PEs controlled by
	 * a single hypervisor or operating system." (see p. 93 of ARM ARM)
	 * We should think what hw configuration we support by one instance of
	 * the hypervisor and choose Inner or Outer sharable domain.
	 */
	ldr	x1, =(T0SZ(48) | (TCR_RGN_WB_WA << TCR_IRGN0_SHIFT)	\
			       | (TCR_RGN_WB_WA << TCR_ORGN0_SHIFT)	\
			       | (TCR_INNER_SHAREABLE << TCR_SH0_SHIFT)	\
			       | (PARANGE_48B << TCR_PS_SHIFT)		\
			       | TCR_EL2_RES1)
	msr	tcr_el2, x1

	msr	ttbr0_el2, x0

	isb
	tlbi	alle2
	dsb	nsh

	/* Enable MMU, allow cacheability for instructions and data */
	ldr	x1, =(SCTLR_I_BIT | SCTLR_C_BIT | SCTLR_M_BIT | SCTLR_EL2_RES1)
	msr	sctlr_el2, x1

	isb
	tlbi	alle2
	dsb	nsh

	ret


	.globl shutdown_el2
shutdown_el2:
	/* x0: struct percpu* */

	/*
	 * Disable the hypervisor MMU.
	 *
	 * Note: no data accesses must be done after turning MMU off unless the
	 * target region has been flushed out of D-cache.
	 */
	mrs	x1, sctlr_el2
	ldr	x2, =(SCTLR_M_BIT | SCTLR_C_BIT | SCTLR_I_BIT)
	bic	x1, x1, x2
	msr	sctlr_el2, x1
	isb

	msr	mair_el2, xzr
	msr	ttbr0_el2, xzr
	msr	tcr_el2, xzr
	isb

	/* Prepare continuation as vmreturn(guest_registers). */
	add	x0, x0, #(PERCPU_STACK_END - 32 * 8)

	/* Fall through to vmreturn */

	.globl vmreturn
vmreturn:
	/* x0: union registers* */
	mov	sp, x0
__vmreturn:
	ldp	x29, x30, [sp, #(15 * 16)]
	ldp	x27, x28, [sp, #(14 * 16)]
	ldp	x25, x26, [sp, #(13 * 16)]
	ldp	x23, x24, [sp, #(12 * 16)]
	ldp	x21, x22, [sp, #(11 * 16)]
	ldp	x19, x20, [sp, #(10 * 16)]
	ldp	x17, x18, [sp, #(9 * 16)]
	ldp	x15, x16, [sp, #(8 * 16)]
	ldp	x13, x14, [sp, #(7 * 16)]
	ldp	x11, x12, [sp, #(6 * 16)]
	ldp	x9, x10, [sp, #(5 * 16)]
	ldp	x7, x8, [sp, #(4 * 16)]
	ldp	x5, x6, [sp, #(3 * 16)]
	ldp	x3, x4, [sp, #(2 * 16)]
	ldp	x1, x2, [sp, #(1 * 16)]
	ldr	    x0, [sp, #(1 * 8)]
	add	sp, sp, #(16 * 16)
	eret
	/*
	 * Mitigate Straight-line Speculation.
	 * Guard against Speculating past an ERET instruction and
	 * potentially perform speculative accesses to memory before
	 * processing the exception return
	 */
	dsb nsh
	isb
	.popsection


	.globl sdei_handler
sdei_handler:
	mov	w0, #1
	strh	w0, [x1, #PERCPU_SDEI_EVENT]

	mrs	x0, vtcr_el2
	orr	x0, x0, #0xff
	msr	vtcr_el2, x0
	isb
	tlbi	vmalls12e1is

	ldr	x0, =SDEI_EVENT_COMPLETE
	mov	x1, #SDEI_EV_HANDLED
	smc	#0

	b	.
