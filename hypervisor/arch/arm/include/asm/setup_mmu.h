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

#ifndef _JAILHOUSE_ASM_SETUP_MMU_H
#define _JAILHOUSE_ASM_SETUP_MMU_H

#include <asm/head.h>
#include <asm/percpu.h>

#ifndef __ASSEMBLY__

/* Procedures used to translate addresses during the MMU setup process */
typedef void* (*phys2virt_t)(unsigned long);
typedef unsigned long (*virt2phys_t)(volatile const void *);

static void __attribute__((naked)) __attribute__((noinline))
cpu_switch_el2(unsigned long phys_bootstrap, virt2phys_t virt2phys)
{
	asm volatile(
	/*
	 * The linux hyp stub allows to install the vectors with a single hvc.
	 * The vector base address is in r0 (phys_bootstrap).
	 */
	"hvc	#0\n"

	/*
	 * Now that the bootstrap vectors are installed, call setup_el2 with
	 * the translated physical values of lr and sp as arguments
	 */
	"mov	r0, sp\n"
	"push	{lr}\n"
	"blx	%0\n"
	"pop	{lr}\n"
	"push	{r0}\n"
	"mov	r0, lr\n"
	"blx	%0\n"
	"pop	{r1}\n"
	"hvc	#0\n"
	:
	: "r" (virt2phys)
	/*
	 * The call to virt2phys may clobber all temp registers. This list
	 * ensures that the compiler uses a decent register for hvirt2phys.
	 */
	: "cc", "memory", "r0", "r1", "r2", "r3");
}

static inline void  __attribute__((always_inline))
cpu_switch_phys2virt(phys2virt_t phys2virt)
{
	/* phys2virt is allowed to touch the stack */
	asm volatile(
	"mov	r0, lr\n"
	"blx	%0\n"
	/* Save virt_lr */
	"push	{r0}\n"
	/* Translate phys_sp */
	"mov	r0, sp\n"
	"blx	%0\n"
	/* Jump back to virtual addresses */
	"mov	sp, r0\n"
	"pop	{pc}\n"
	:
	: "r" (phys2virt)
	: "cc", "r0", "r1", "r2", "r3", "lr", "sp");
}

#endif /* !__ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_SETUP_MMU_H */
