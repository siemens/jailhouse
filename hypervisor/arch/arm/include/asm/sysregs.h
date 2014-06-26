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

#ifndef _JAILHOUSE_ASM_SYSREGS_H
#define _JAILHOUSE_ASM_SYSREGS_H

/*
 * Along with some system register names, this header defines the following
 * macros for accessing cp15 registers.
 *
 * C-side:
 * - arm_write_sysreg(SYSREG_NAME, var)
 * - arm_read_sysreg(SYSREG_NAME, var)
 * asm-side:
 * - arm_write_sysreg(SYSREG_NAME, reg)
 * - arm_read_sysreg(SYSREG_NAME, reg)
 */

/*
 * 32bit sysregs definitions
 * (Use the AArch64 names to ease the compatibility work)
 */
#define MPIDR_EL1	SYSREG_32(0, c0, c0, 5)
#define ID_PFR0_EL1	SYSREG_32(0, c0, c1, 0)
#define ID_PFR1_EL1	SYSREG_32(0, c0, c1, 1)
#define SCTLR_EL2	SYSREG_32(4, c1, c0, 0)
#define ESR_EL2		SYSREG_32(4, c5, c2, 0)
#define TPIDR_EL2	SYSREG_32(4, c13, c0, 2)
#define TTBR0_EL2	SYSREG_64(4, c2)
#define TCR_EL2		SYSREG_32(4, c2, c0, 2)
#define VTTBR_EL2	SYSREG_64(6, c2)
#define VTCR_EL2	SYSREG_32(4, c2, c1, 2)

#define PAR_EL1		SYSREG_64(0, c7)

/* AArch32-specific registers */
#define HCR		SYSREG_32(4, c1, c1, 0)
#define HCR2		SYSREG_32(4, c1, c1, 4)
#define HMAIR0		SYSREG_32(4, c10, c2, 0)
#define HMAIR1		SYSREG_32(4, c10, c2, 1)
#define HVBAR		SYSREG_32(4, c12, c0, 0)

#define ATS1HR		SYSREG_32(4, c7, c8, 0)

#define TLBIALL		SYSREG_32(0, c8, c7, 0)
#define TLBIALLIS	SYSREG_32(0, c8, c3, 0)
#define TLBIASID	SYSREG_32(0, c8, c7, 2)
#define TLBIASIDIS	SYSREG_32(0, c8, c3, 2)
#define TLBIMVA		SYSREG_32(0, c8, c7, 1)
#define TLBIMVAIS	SYSREG_32(0, c8, c3, 1)
#define TLBIMVAL	SYSREG_32(0, c8, c7, 5)
#define TLBIMVALIS	SYSREG_32(0, c8, c3, 5)
#define TLBIMVAA	SYSREG_32(0, c8, c7, 3)
#define TLBIMVAAIS	SYSREG_32(0, c8, c3, 3)
#define TLBIMVAAL	SYSREG_32(0, c8, c7, 7)
#define TLBIMVAALIS	SYSREG_32(0, c8, c3, 7)
#define TLBIALLH	SYSREG_32(4, c8, c7, 0)
#define TLBIALLHIS	SYSREG_32(4, c8, c3, 0)
#define TLBIALLNSNH	SYSREG_32(4, c8, c7, 4)
#define TLBIALLNSNHIS	SYSREG_32(4, c8, c3, 4)
#define TLBIMVAH	SYSREG_32(4, c8, c7, 1)
#define TLBIMVAHIS	SYSREG_32(4, c8, c3, 1)
#define TLBIMVALH	SYSREG_32(4, c8, c7, 5)
#define TLBIMVALHIS	SYSREG_32(4, c8, c3, 5)
#define TLBIIPAS2	SYSREG_32(4, c8, c4, 1)
#define TLBIIPAS2IS	SYSREG_32(4, c8, c0, 1)
#define TLBIIPAS2L	SYSREG_32(4, c8, c5, 5)
#define TLBIIPAS2LIS	SYSREG_32(4, c8, c0, 5)

#define SYSREG_32(...) 32, __VA_ARGS__
#define SYSREG_64(...) 64, __VA_ARGS__

#define _arm_write_sysreg(size, ...) arm_write_sysreg_ ## size(__VA_ARGS__)
#define arm_write_sysreg(...) _arm_write_sysreg(__VA_ARGS__)

#define _arm_read_sysreg(size, ...) arm_read_sysreg_ ## size(__VA_ARGS__)
#define arm_read_sysreg(...) _arm_read_sysreg(__VA_ARGS__)

#ifndef __ASSEMBLY__

#define arm_write_sysreg_32(op1, crn, crm, op2, val) \
	asm volatile ("mcr	p15, "#op1", %0, "#crn", "#crm", "#op2"\n" \
			: : "r"((u32)(val)))
#define arm_write_sysreg_64(op1, crm, val) \
	asm volatile ("mcrr	p15, "#op1", %Q0, %R0, "#crm"\n" \
			: : "r"((u64)(val)))

#define arm_read_sysreg_32(op1, crn, crm, op2, val) \
	asm volatile ("mrc	p15, "#op1", %0, "#crn", "#crm", "#op2"\n" \
			: "=r"((u32)(val)))
#define arm_read_sysreg_64(op1, crm, val) \
	asm volatile ("mrrc	p15, "#op1", %Q0, %R0, "#crm"\n" \
			: "=r"((u64)(val)))

#else /* __ASSEMBLY__ */

#define arm_write_sysreg_32(op1, crn, crm, op2, reg) \
	mcr	p15, op1, reg, crn, crm, op2
#define arm_write_sysreg_64(op1, crm, reg1, reg2) \
	mcrr	p15, op1, reg1, reg2, crm

#define arm_read_sysreg_32(op1, crn, crm, op2, reg) \
	mrc	p15, op1, reg, crn, crm, op2
#define arm_read_sysreg_64(op1, crm, reg1, reg2) \
	mrrc	p15, op1, reg1, reg2, crm

#endif /* __ASSEMBLY__ */

#endif
