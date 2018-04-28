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

/* Processor Status Register definitions */
#define PSR_MODE_MASK	0xf
#define PSR_USR_MODE	0x0
#define PSR_FIQ_MODE	0x1
#define PSR_IRQ_MODE	0x2
#define PSR_SVC_MODE	0x3
#define PSR_MON_MODE	0x6
#define PSR_ABT_MODE	0x7
#define PSR_HYP_MODE	0xa
#define PSR_UND_MODE	0xb
#define PSR_SYS_MODE	0xf

#define PSR_32_BIT	(1 << 4)
#define PSR_T_BIT	(1 << 5)
#define PSR_F_BIT	(1 << 6)
#define PSR_I_BIT	(1 << 7)
#define PSR_A_BIT	(1 << 8)
#define PSR_E_BIT	(1 << 9)
#define PSR_J_BIT	(1 << 24)
#define PSR_IT_MASK(it)	(((it) & 0x3) << 25 | ((it) & 0xfc) << 8)
#define PSR_IT(psr)	(((psr) >> 25 & 0x3) | ((psr) >> 8 & 0xfc))

#define RESET_PSR	(PSR_I_BIT | PSR_F_BIT | PSR_A_BIT | PSR_SVC_MODE \
			| PSR_32_BIT)

/*
 * 32bit sysregs definitions
 * (Use the AArch64 names to ease the compatibility work)
 */
#define CTR_EL0		SYSREG_32(0, c0, c0, 1)
#define MPIDR_EL1	SYSREG_32(0, c0, c0, 5)
#define  MPIDR_CPUID_MASK	0x00ffffff
#define  MPIDR_CLUSTERID_MASK	0x00ffff00
#define  MPIDR_AFF0_MASK	0x000000ff
#define  MPIDR_U_BIT		(1 << 30)
#define  MPIDR_MP_BIT		(1 << 31)
#define ID_PFR0_EL1	SYSREG_32(0, c0, c1, 0)
#define ID_PFR1_EL1	SYSREG_32(0, c0, c1, 1)
#define  PFR1_VIRT(pfr)		((pfr) >> 12 & 0xf)
#define SCTLR_EL1	SYSREG_32(0, c1, c0, 0)
#define  SCTLR_M_BIT		(1 << 0)
#define  SCTLR_A_BIT		(1 << 1)
#define  SCTLR_C_BIT		(1 << 2)
#define  SCTLR_CP15B_BIT	(1 << 5)
#define  SCTLR_ITD_BIT		(1 << 7)
#define  SCTLR_SED_BIT		(1 << 8)
#define  SCTLR_I_BIT		(1 << 12)
#define  SCTLR_V_BIT		(1 << 13)
#define  SCTLR_nTWI		(1 << 16)
#define  SCTLR_nTWE		(1 << 18)
#define  SCTLR_WXN_BIT		(1 << 19)
#define  SCTLR_UWXN_BIT		(1 << 20)
#define  SCTLR_FI_BIT		(1 << 21)
#define  SCTLR_EE_BIT		(1 << 25)
#define  SCTLR_TRE_BIT		(1 << 28)
#define  SCTLR_AFE_BIT		(1 << 29)
#define  SCTLR_TE_BIT		(1 << 30)
#define  SCTLR_C_AND_M_SET(sctlr)	\
	(((sctlr) & (SCTLR_C_BIT | SCTLR_M_BIT)) == (SCTLR_C_BIT | SCTLR_M_BIT))

#define MPIDR_LEVEL_BITS		8
#define MPIDR_LEVEL_MASK		((1 << MPIDR_LEVEL_BITS) - 1)
#define MPIDR_LEVEL_SHIFT(level)	(MPIDR_LEVEL_BITS * (level))

#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
	(((mpidr) >> (MPIDR_LEVEL_BITS * (level))) & MPIDR_LEVEL_MASK)

/* Bits to wipe on cell reset */
#define  SCTLR_MASK	(SCTLR_M_BIT | SCTLR_A_BIT | SCTLR_C_BIT	\
			| SCTLR_I_BIT | SCTLR_V_BIT | SCTLR_WXN_BIT	\
			| SCTLR_UWXN_BIT | SCTLR_FI_BIT | SCTLR_EE_BIT	\
			| SCTLR_TRE_BIT | SCTLR_AFE_BIT | SCTLR_TE_BIT)
#define ACTLR_EL1	SYSREG_32(0, c1, c0, 1)
#define CPACR_EL1	SYSREG_32(0, c1, c0, 2)
#define CONTEXTIDR_EL1	SYSREG_32(0, c13, c0, 1)
#define CSSIDR_EL1	SYSREG_32(1, c0, c0, 0)
#define CLIDR_EL1	SYSREG_32(1, c0, c0, 1)
#define CSSELR_EL1	SYSREG_32(2, c0, c0, 0)
#define SCTLR_EL2	SYSREG_32(4, c1, c0, 0)
#define HSR		SYSREG_32(4, c5, c2, 0)
/* exception class */
#define  HSR_EC(hsr)		GET_FIELD((hsr), 31, 26)
/* instruction length */
#define  HSR_IL(hsr)		GET_FIELD((hsr), 25, 25)
/* Instruction specific syndrome */
#define  HSR_ISS(hsr)		GET_FIELD((hsr), 24, 0)
/* Exception classes values */
#define  HSR_EC_UNK		0x00
#define  HSR_EC_WFI		0x01
#define  HSR_EC_CP15_32		0x03
#define  HSR_EC_CP15_64		0x04
#define  HSR_EC_CP14_32		0x05
#define  HSR_EC_CP14_LC		0x06
#define  HSR_EC_HCPTR		0x07
#define  HSR_EC_CP10		0x08
#define  HSR_EC_CP14_64		0x0c
#define  HSR_EC_SVC_HYP		0x11
#define  HSR_EC_HVC		0x12
#define  HSR_EC_SMC		0x13
#define  HSR_EC_IABT		0x20
#define  HSR_EC_IABT_HYP		0x21
#define  HSR_EC_PCALIGN		0x22
#define  HSR_EC_DABT		0x24
#define  HSR_EC_DABT_HYP		0x25
/* Condition code */
#define  HSR_ISS_CV_BIT		(1 << 24)
#define  HSR_ISS_COND(iss)	((iss) >> 20 & 0xf)

#define  HSR_MATCH_MCR_MRC(hsr, crn, opc1, crm, opc2)		\
	(((hsr) & (BIT_MASK(19, 10) | BIT_MASK(4, 1))) ==	\
	 (((opc2) << 17) | ((opc1) << 14) | ((crn) << 10) | ((crm) << 1)))

#define  HSR_MATCH_MCRR_MRRC(hsr, opc1, crm)			\
	(((hsr) & (BIT_MASK(19, 16) | BIT_MASK(4, 1))) ==	\
	 (((opc1) << 16) | ((crm) << 1)))

#define TTBR0_EL2	SYSREG_64(4, c2)
#define TCR_EL2		SYSREG_32(4, c2, c0, 2)
#define VTTBR_EL2	SYSREG_64(6, c2)
#define VTCR_EL2	SYSREG_32(4, c2, c1, 2)

#define TTBR0_EL1	SYSREG_64(0, c2)
#define TTBR1_EL1	SYSREG_64(1, c2)
#define PAR_EL1		SYSREG_64(0, c7)
#define  PAR_F_BIT	0x1
#define  PAR_FST_SHIFT	1
#define  PAR_FST_MASK	0x3f
#define  PAR_SHA_SHIFT	7
#define  PAR_SHA_MASK	0x3
#define  PAR_NS_BIT	(0x1 << 9)
#define  PAR_LPAE_BIT	(0x1 << 11)
#define  PAR_PA_MASK	BIT_MASK(39, 12)
#define  PAR_ATTR_SHIFT	56
#define  PAR_ATTR_MASK	0xff

#define CNTKCTL_EL1	SYSREG_32(0, c14, c1, 0)
#define CNTP_TVAL_EL0	SYSREG_32(0, c14, c2, 0)
#define CNTP_CTL_EL0	SYSREG_32(0, c14, c2, 1)
#define CNTP_CVAL_EL0	SYSREG_64(2, c14)
#define CNTV_TVAL_EL0	SYSREG_32(0, c14, c3, 0)
#define CNTV_CTL_EL0	SYSREG_32(0, c14, c3, 1)
#define CNTV_CVAL_EL0	SYSREG_64(3, c14)

#define CNTPCT_EL0	SYSREG_64(0, c14)

/*
 * AArch32-specific registers: they are 64bit on AArch64, and will need some
 * helpers if used frequently.
 */
#define TTBCR		SYSREG_32(0, c2, c0, 2)
#define DACR		SYSREG_32(0, c3, c0, 0)
#define VBAR		SYSREG_32(0, c12, c0, 0)
#define HCR		SYSREG_32(4, c1, c1, 0)
#define HCR2		SYSREG_32(4, c1, c1, 4)
#define  HCR_TRVM_BIT	(1 << 30)
#define  HCR_TVM_BIT	(1 << 26)
#define  HCR_HDC_BIT	(1 << 29)
#define  HCR_TGE_BIT	(1 << 27)
#define  HCR_TTLB_BIT	(1 << 25)
#define  HCR_TPU_BIT	(1 << 24)
#define  HCR_TPC_BIT	(1 << 23)
#define  HCR_TSW_BIT	(1 << 22)
#define  HCR_TAC_BIT	(1 << 21)
#define  HCR_TIDCP_BIT	(1 << 20)
#define  HCR_TSC_BIT	(1 << 19)
#define  HCR_TID3_BIT	(1 << 18)
#define  HCR_TID2_BIT	(1 << 17)
#define  HCR_TID1_BIT	(1 << 16)
#define  HCR_TID0_BIT	(1 << 15)
#define  HCR_TWE_BIT	(1 << 14)
#define  HCR_TWI_BIT	(1 << 13)
#define  HCR_DC_BIT	(1 << 12)
#define  HCR_BSU_BITS	(3 << 10)
#define  HCR_BSU_INNER	(1 << 10)
#define  HCR_BSU_OUTER	(2 << 10)
#define  HCR_BSU_FULL	HCR_BSU_BITS
#define  HCR_FB_BIT	(1 << 9)
#define  HCR_VA_BIT	(1 << 8)
#define  HCR_VI_BIT	(1 << 7)
#define  HCR_VF_BIT	(1 << 6)
#define  HCR_AMO_BIT	(1 << 5)
#define  HCR_IMO_BIT	(1 << 4)
#define  HCR_FMO_BIT	(1 << 3)
#define  HCR_PTW_BIT	(1 << 2)
#define  HCR_SWIO_BIT	(1 << 1)
#define  HCR_VM_BIT	(1 << 0)
#define HDFAR		SYSREG_32(4, c6, c0, 0)
#define HIFAR		SYSREG_32(4, c6, c0, 2)
#define HPFAR		SYSREG_32(4, c6, c0, 4)
#define HMAIR0		SYSREG_32(4, c10, c2, 0)
#define HMAIR1		SYSREG_32(4, c10, c2, 1)
#define HVBAR		SYSREG_32(4, c12, c0, 0)

/* Mapped to HSR, IFSR32 and FAR in AArch64 */
#define DFSR		SYSREG_32(0, c5, c0, 0)
#define DFAR		SYSREG_32(0, c6, c0, 0)
#define IFSR		SYSREG_32(0, c5, c0, 1)
#define IFAR		SYSREG_32(0, c6, c0, 2)
#define ADFSR		SYSREG_32(0, c5, c1, 0)
#define AIFSR		SYSREG_32(0, c5, c1, 1)

/* Mapped to MAIR_EL1 */
#define MAIR0		SYSREG_32(0, c10, c2, 0)
#define MAIR1		SYSREG_32(0, c10, c2, 1)
#define AMAIR0		SYSREG_32(0, c10, c3, 0)
#define AMAIR1		SYSREG_32(0, c10, c3, 1)

#define TPIDRURW	SYSREG_32(0, c13, c0, 2)
#define TPIDRURO	SYSREG_32(0, c13, c0, 3)
#define TPIDRPRW	SYSREG_32(0, c13, c0, 4)

#define CNTFRQ_EL0	SYSREG_32(0, c14, c0, 0)

#define ATS1HR		SYSREG_32(4, c7, c8, 0)

#define ICIALLUIS	SYSREG_32(0, c7, c1, 0)
#define ICIALLU		SYSREG_32(0, c7, c5, 0)
#define DCIMVAC		SYSREG_32(0, c7, c6, 1)
#define DCCMVAC		SYSREG_32(0, c7, c10, 1)
#define DCCSW		SYSREG_32(0, c7, c10, 2)
#define DCCIMVAC	SYSREG_32(0, c7, c14, 1)
#define DCCISW		SYSREG_32(0, c7, c14, 2)

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
asm(".arch_extension virt\n");

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

#define arm_read_banked_reg(reg, val) \
	asm volatile ("mrs %0, " #reg "\n" : "=r" (val))

#define arm_write_banked_reg(reg, val) \
	asm volatile ("msr " #reg ", %0\n" : : "r" (val))

#define arm_rw_banked_reg(reg, val, is_read)		\
	do {						\
		if (is_read)				\
			arm_read_banked_reg(reg, val);	\
		else					\
			arm_write_banked_reg(reg, val);	\
	} while (0)

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
