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
 * hypervisor/arch/arm64/include/asm/sysregs.h */

#ifndef __ASSEMBLY__

#include <string.h>

#define SCTLR_EL1_I	(1 << 12)
#define SCTLR_EL1_C	(1 << 2)
#define SCTLR_EL1_M	(1 << 0)

#define SCTLR		SCTLR_EL1

/* Enable MMU, data+instruction caches */
#define SCTLR_MMU_CACHES	(SCTLR_EL1_I | SCTLR_EL1_C | SCTLR_EL1_M)

#define TCR_EL1_T0SZ_25		25
#define TCR_EL1_IRGN0_WBWAC	(0x1 << 8)
#define TCR_EL1_ORGN0_WBWAC	(0x1 << 10)
#define TCR_EL1_SH0_IS		(0x3 << 12)
#define TCR_EL1_TG0_4K		(0x0 << 14)
#define TCR_EL1_IPC_256TB	(0x5UL << 32)

/*
 * IPA size 48bit (256TiB), 4KiB granularity, and set inner/outer caches to
 * write-back write-allocate cacheable and shareability attribute to inner
 * shareable
 */
#define TRANSL_CONT_REG TCR_EL1
#define TRANSL_CONT_REG_SETTINGS \
	TCR_EL1_IPC_256TB | TCR_EL1_TG0_4K | TCR_EL1_SH0_IS | \
	TCR_EL1_ORGN0_WBWAC | TCR_EL1_IRGN0_WBWAC | TCR_EL1_T0SZ_25

#define MAIR	MAIR_EL1

#define TTBR0	TTBR0_EL1

#define MPIDR	MPIDR_EL1

#define MPIDR_LEVEL_BITS_SHIFT	3
#define MPIDR_LEVEL_BITS	(1 << MPIDR_LEVEL_BITS_SHIFT)
#define MPIDR_LEVEL_MASK	((1 << MPIDR_LEVEL_BITS) - 1)

#define MPIDR_LEVEL_SHIFT(level) \
        (((1 << (level)) >> 1) << MPIDR_LEVEL_BITS_SHIFT)

#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
        (((mpidr) >> MPIDR_LEVEL_SHIFT(level)) & MPIDR_LEVEL_MASK)

#define SYSREG_32(op1, crn, crm, op2)	s3_##op1 ##_##crn ##_##crm ##_##op2

#define arm_write_sysreg(sysreg, val) \
	asm volatile ("msr	"__stringify(sysreg)", %0\n" : : "r"((u64)(val)))

#define arm_read_sysreg(sysreg, val) \
	asm volatile ("mrs	%0,  "__stringify(sysreg)"\n" : "=r"((val)))

#include <asm/sysregs_common.h>

#endif /* __ASSEMBLY__ */
