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

static inline void  __attribute__((always_inline))
cpu_switch_phys2virt(phys2virt_t phys2virt)
{
	/* phys2virt is allowed to touch the stack */
	asm volatile(
		"mov	r0, lr\n\t"
		"blx	%0\n\t"
		/* Save virt_lr */
		"push	{r0}\n\t"
		/* Translate phys_sp */
		"mov	r0, sp\n\t"
		"blx	%0\n\t"
		/* Jump back to virtual addresses */
		"mov	sp, r0\n\t"
		"pop	{pc}\n\t"
		:
		: "r" (phys2virt)
		: "cc", "r0", "r1", "r2", "r3", "lr", "sp");
}

#endif /* !__ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_SETUP_MMU_H */
