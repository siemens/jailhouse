/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2017
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* The following definitions are inspired by
 * hypervisor/arch/arm/include/asm/sysregs.h */

#define VBAR		SYSREG_32(0, c12, c0, 0)
#define CNTFRQ_EL0	SYSREG_32(0, c14, c0, 0)
#define CNTV_TVAL_EL0	SYSREG_32(0, c14, c3, 0)
#define CNTV_CTL_EL0	SYSREG_32(0, c14, c3, 1)
#define CNTPCT_EL0	SYSREG_64(0, c14)

#define SCTLR		SYSREG_32(0, c1, c0, 0)
#define  SCTLR_RR	(1 << 14)
#define  SCTLR_I	(1 << 12)
#define  SCTLR_C	(1 << 2)
#define  SCTLR_M	(1 << 0)

/* Enable MMU, round-robin replacement, data+instruction caches */
#define SCTLR_MMU_CACHES	(SCTLR_RR | SCTLR_I | SCTLR_C | SCTLR_M)

#define TTBR0		SYSREG_32(0, c2, c0, 0)
#define TTBCR		SYSREG_32(0, c2, c0, 2)
#define  TTBCR_IRGN0_WB_WA		(1 << 8)
#define  TTBCR_ORGN0_WB_WA		(1 << 10)
#define  TTBCR_SH0_INNER_SHAREABLE	(3 << 12)
#define  TTBCR_EAE			(1 << 31)

/*
 * Enable extended address enable and set inner/outer caches to write-back
 * write-allocate cacheable and shareability attribute to inner shareable
 */
#define TRANSL_CONT_REG TTBCR
#define TRANSL_CONT_REG_SETTINGS \
	TTBCR_EAE | TTBCR_IRGN0_WB_WA | TTBCR_ORGN0_WB_WA | \
	TTBCR_SH0_INNER_SHAREABLE

#define MAIR0		SYSREG_32(0, c10, c2, 0)
#define MAIR1		SYSREG_32(0, c10, c2, 1)

#define MAIR MAIR0

#define MPIDR		SYSREG_32(0, c0, c0, 5)

#define  MPIDR_CPUID_MASK	0x00ffffff

#define MPIDR_LEVEL_BITS		8
#define MPIDR_LEVEL_MASK		((1 << MPIDR_LEVEL_BITS) - 1)
#define MPIDR_LEVEL_SHIFT(level)	(MPIDR_LEVEL_BITS * (level))

#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
	(((mpidr) >> (MPIDR_LEVEL_BITS * (level))) & MPIDR_LEVEL_MASK)

#define SYSREG_32(...) 32, __VA_ARGS__
#define SYSREG_64(...) 64, __VA_ARGS__

#define _arm_write_sysreg(size, ...) arm_write_sysreg_ ## size(__VA_ARGS__)
#define arm_write_sysreg(...) _arm_write_sysreg(__VA_ARGS__)

#define _arm_read_sysreg(size, ...) arm_read_sysreg_ ## size(__VA_ARGS__)
#define arm_read_sysreg(...) _arm_read_sysreg(__VA_ARGS__)

#include <asm/sysregs_common.h>

#ifndef __ASSEMBLY__
asm(".arch_extension virt\n");

#define arm_write_sysreg_32(op1, crn, crm, op2, val) \
	asm volatile ("mcr	p15, "#op1", %0, "#crn", "#crm", "#op2"\n" \
			: : "r" ((u32)(val)))

#define arm_read_sysreg_32(op1, crn, crm, op2, val) \
	asm volatile ("mrc	p15, "#op1", %0, "#crn", "#crm", "#op2"\n" \
			: "=r" ((u32)(val)))
#define arm_read_sysreg_64(op1, crm, val) \
	asm volatile ("mrrc	p15, "#op1", %Q0, %R0, "#crm"\n" \
			: "=r" ((u64)(val)))

#else /* __ASSEMBLY__ */

#define arm_write_sysreg_32(op1, crn, crm, op2, reg) \
	mcr	p15, op1, reg, crn, crm, op2

#endif /* __ASSEMBLY__ */
