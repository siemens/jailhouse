/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015-2016 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/asm-defines.h>
#include <asm/paging.h>
#include <asm/percpu.h>
#include <asm/jailhouse_hypercall.h>

/* Entry point for Linux loader module on JAILHOUSE_ENABLE */
	.text
	.globl arch_entry
arch_entry:
	/*
	 * x0: cpuid
	 *
	 * We don't have access to our own address space yet, so we will
	 * abuse some caller saved registers to preserve across calls:
	 * x15: physical UART address
	 * x16: saved hyp vectors
	 * x17: cpuid
	 * x18: caller lr
	 */
	mov	x17, x0
	mov	x18, x30

	/*
	 * Access the just updated hypervisor_header prior to turning off the
	 * MMU. Later, we will only read a stale memory content.
	 */
	adr     x15, hypervisor_header
	ldr     x15, [x15, #DEBUG_CONSOLE_BASE]

	/* Note 1: After turning MMU off the CPU can start bypassing caches.
	 * But cached before data is kept in caches either until the CPU turns
	 * MMU on again or other coherent agents move cached data out. That's
	 * why there is no need to clean D-cache before turning MMU off.
	 *
	 * Note 2: We don't have to clean D-cache to protect against malicious
	 * guests, which can execute 'dc isw' (data or unified Cache line
	 * Invalidate by Set/Way) because when virtualization is enabled
	 * (HCR_EL2.VM == 1) then HW automatically upgrade 'dc isw' to
	 * 'dc cisw' (Clean + Invallidate). Executing Clean operation before
	 * Invalidate is safe in guests.
	 */

	/* keep the linux stub EL2 vectors for later */
	mov	x0, xzr
	hvc	#0
	mov	x16, x0

	/* install bootstrap_vectors */
	ldr	x0, =bootstrap_vectors
	hvc	#0
	hvc	#0	/* bootstrap vectors enter EL2 at el2_entry */
	b	.	/* we don't expect to return here */

	/* the bootstrap vector returns us here in physical addressing */
el2_entry:
	mrs	x1, esr_el2
	lsr	x1, x1, #26
	cmp	x1, #0x16
	b.ne	.		/* not hvc */

	/* install jailhouse vectors */
	adr	x1, hyp_vectors
	msr	vbar_el2, x1

	/* init bootstrap page tables */
	bl	init_bootstrap_pt

	/* enable temporary mmu mapings for early initialization */
	adr	x0, bootstrap_pt_l0
	bl	enable_mmu_el2

	mov	x0, x17		/* preserved cpuid, will be passed to entry */
	adrp	x1, __page_pool
	mov	x2, #(1 << PERCPU_SIZE_SHIFT_ASM)
	/*
	 * percpu data = pool + cpuid * shift
	 */
	madd	x1, x2, x0, x1
	msr	tpidr_el2, x1

	/* set up the stack and push the root cell's callee saved registers */
	add	sp, x1, #PERCPU_STACK_END
	stp	x29, x18, [sp, #-16]!	/* note: our caller lr is in x18 */
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

	mov	x29, xzr	/* reset fp,lr */
	mov	x30, xzr

	/* save the Linux stub vectors we kept earlier */
	add	x2, x1, #PERCPU_SAVED_VECTORS
	str	x16, [x2]

	/* Call entry(cpuid, struct per_cpu*). Should not return. */
	bl	entry
	b	.

	.globl arch_shutdown_mmu
arch_shutdown_mmu:
	/* x0: struct percpu* */
	mov	x19, x0

	/* Note: no memory accesses must be done after turning MMU off. There
	 * is non-zero probability that cached data can be not syncronized with
	 * system memory. CPU can access data bypassing D-cache when MMU is off.
	 */

	/* hand over control of EL2 back to Linux */
	add	x1, x19, #PERCPU_SAVED_VECTORS
	ldr	x2, [x1]
	msr	vbar_el2, x2

	/* disable the hypervisor MMU */
	mrs	x1, sctlr_el2
	ldr	x2, =(SCTLR_M_BIT | SCTLR_C_BIT | SCTLR_I_BIT)
	bic	x1, x1, x2
	msr	sctlr_el2, x1
	isb

	msr	mair_el2, xzr
	msr	ttbr0_el2, xzr
	msr	tcr_el2, xzr
	isb

	msr	tpidr_el2, xzr

	/* Call vmreturn(guest_registers) */
	add	x0, x19, #(PERCPU_STACK_END - 32 * 8)
	b	vmreturn

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
	 * the hypervisor and choose Inner or Outter sharable domain.
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
	 * x0: physical address of hypervisor binary (2mb block)
	 * x1: physical address of uart to map (2mb block)
	 *
	 * These are referenced statically for now.
	 * AARCH64_TODO: remove the build time dependency, and take
	 * these values as input from the system configuration.
	 *
	 * Clobbers x0-x4,x8,x9
	 */
	ldr	x0, =JAILHOUSE_BASE
	mov	x1, x15

	/* l0 pt index for firmware and uart */
	get_index x2, x0, 0
	get_index x3, x1, 0

	/* map the l1 table that includes the firmware */
	set_table bootstrap_pt_l0, x2, bootstrap_pt_l1

	cmp	x2, x3
	b.eq	1f

	/*
	 * Case 1: firmware and uart reside on sepparate l0 entries
	 *	   (512gb regions). The wildcard table is used as an
	 *	   l1 table for the uart.
	 */
	get_index x2, x0, 1
	set_block bootstrap_pt_l1, x2, x0, 1 /* 1gb block for firmware */

	/* 512gb blocks are not supported by the hardware. Use the
	 * wildcard table to map a 1gb block for the uart */
	set_table bootstrap_pt_l0, x3, bootstrap_pt_wildcard
	get_index x3, x1, 1
	set_block_dev bootstrap_pt_wildcard, x3, x1, 1

	b	flush

1:	get_index x2, x0, 1
	get_index x3, x1, 1
	cmp	x2, x3
	b.eq	1f

	/*
	 * Case 2: firwmare and uart reside on sepparate l1 entries.
	 *	   Just map 1gb blocks, we don't need the wildcard.
	 */
	set_block bootstrap_pt_l1, x2, x0, 1
	set_block_dev bootstrap_pt_l1, x3, x1, 1

	b	flush

	/* l1 granularity not enough; attempt to map on l2 blocks (2mb) */
1:	set_table bootstrap_pt_l1, x2, bootstrap_pt_wildcard
	get_index x2, x0, 2
	get_index x3, x1, 2
	cmp	x2, x3
	b.eq	1f

	/*
	 * Case 3: firmware and uart reside on sepparate l2 entries,
	 *	   we can still salvage the situation (2mb blocks).
	 *	   We use the wildcard table for the l2 table for
	 *	   the firmware and the uart.
	 */
	set_block bootstrap_pt_wildcard, x2, x0, 2
	set_block_dev bootstrap_pt_wildcard, x3, x1, 2
	b	flush

	/* uart and firmware within same 2MB block; cry now */
1:	b	.

flush:	adr	x0, bootstrap_pt_l0
	mov	x1, PAGE_SIZE * 3
	mov	x2, DCACHE_CLEAN_AND_INVALIDATE_ASM
	b	arm_dcaches_flush	// will ret to caller

.macro	ventry	label
	.align	7
	b	\label
.endm

	.globl bootstrap_vectors
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
