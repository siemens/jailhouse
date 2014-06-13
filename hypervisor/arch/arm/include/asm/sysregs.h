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
#define TPIDR_EL2	SYSREG_32(4, c13, c0, 2)

#define HVBAR		SYSREG_32(4, c12, c0, 0)

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
