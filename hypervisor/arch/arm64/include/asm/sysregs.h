/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_SYSREGS_H
#define _JAILHOUSE_ASM_SYSREGS_H

#define PSR_MODE_MASK	0xf
#define PSR_MODE_EL0t	0x0
#define PSR_MODE_SVC	0x3
#define PSR_MODE_EL1t	0x4
#define PSR_MODE_EL1h	0x5
#define PSR_MODE_EL2t	0x8
#define PSR_MODE_EL2h	0x9

#define PSR_32_BIT		(1 << 4)
#define PSR_F_BIT		(1 << 6)
#define PSR_I_BIT		(1 << 7)
#define PSR_A_BIT		(1 << 8)
#define PSR_D_BIT		(1 << 9)
#define PSR_IL_BIT		(1 << 20)
#define PSR_SS_BIT		(1 << 21)
#define RESET_PSR_AARCH64	(PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | PSR_F_BIT \
				| PSR_MODE_EL1h)
#define RESET_PSR_AARCH32	(PSR_A_BIT | PSR_I_BIT | PSR_F_BIT \
				| PSR_32_BIT | PSR_MODE_SVC)

#define MPIDR_CPUID_MASK	0xff00ffffffUL
#define MPIDR_CLUSTERID_MASK	0xff00ffff00UL
#define MPIDR_AFF0_MASK		0x00000000ffUL
#define MPIDR_U_BIT		(1 << 30)
#define MPIDR_MP_BIT		(1 << 31)

#define MPIDR_LEVEL_BITS_SHIFT	3
#define MPIDR_LEVEL_BITS	(1 << MPIDR_LEVEL_BITS_SHIFT)
#define MPIDR_LEVEL_MASK	((1 << MPIDR_LEVEL_BITS) - 1)

#define MPIDR_LEVEL_SHIFT(level) \
	(((1 << (level)) >> 1) << MPIDR_LEVEL_BITS_SHIFT)

#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
	(((mpidr) >> MPIDR_LEVEL_SHIFT(level)) & MPIDR_LEVEL_MASK)

#define SCTLR_M_BIT	(1 << 0)
#define SCTLR_A_BIT	(1 << 1)
#define SCTLR_C_BIT	(1 << 2)
#define SCTLR_SA_BIT	(1 << 3)
#define SCTLR_SA0_BIT	(1 << 4)
#define SCTLR_CP15B_BIT (1 << 5)
#define SCTLR_ITD_BIT	(1 << 7)
#define SCTLR_SED_BIT	(1 << 8)
#define SCTLR_UMA_BIT	(1 << 9)
#define SCTLR_I_BIT	(1 << 12)
#define SCTLR_DZE_BIT	(1 << 14)
#define SCTLR_UCT_BIT	(1 << 15)
#define SCTLR_nTWI	(1 << 16)
#define SCTLR_nTWE	(1 << 18)
#define SCTLR_WXN_BIT	(1 << 19)
#define SCTLR_E0E_BIT	(1 << 24)
#define SCTLR_EE_BIT	(1 << 25)
#define SCTLR_UCI_BIT	(1 << 26)

#define SCTLR_EL1_RES1	((1 << 11) | (1 << 20) | (3 << 22) | (3 << 28))
#define SCTLR_EL2_RES1	((3 << 4) | (1 << 11) | (1 << 16) | (1 << 18)	\
			| (3 << 22) | (3 << 28))

#define HCR_MIOCNCE_BIT	(1u << 38)
#define HCR_ID_BIT	(1u << 33)
#define HCR_CD_BIT	(1u << 32)
#define HCR_RW_BIT	(1u << 31)
#define HCR_TRVM_BIT	(1u << 30)
#define HCR_HDC_BIT	(1u << 29)
#define HCR_TDZ_BIT	(1u << 28)
#define HCR_TGE_BIT	(1u << 27)
#define HCR_TVM_BIT	(1u << 26)
#define HCR_TTLB_BIT	(1u << 25)
#define HCR_TPU_BIT	(1u << 24)
#define HCR_TPC_BIT	(1u << 23)
#define HCR_TSW_BIT	(1u << 22)
#define HCR_TAC_BIT	(1u << 21)
#define HCR_TIDCP_BIT	(1u << 20)
#define HCR_TSC_BIT	(1u << 19)
#define HCR_TID3_BIT	(1u << 18)
#define HCR_TID2_BIT	(1u << 17)
#define HCR_TID1_BIT	(1u << 16)
#define HCR_TID0_BIT	(1u << 15)
#define HCR_TWE_BIT	(1u << 14)
#define HCR_TWI_BIT	(1u << 13)
#define HCR_DC_BIT	(1u << 12)
#define HCR_BSU_BITS	(3u << 10)
#define HCR_BSU_INNER	(1u << 10)
#define HCR_BSU_OUTER	(2u << 10)
#define HCR_BSU_FULL	HCR_BSU_BITS
#define HCR_FB_BIT	(1u << 9)
#define HCR_VA_BIT	(1u << 8)
#define HCR_VI_BIT	(1u << 7)
#define HCR_VF_BIT	(1u << 6)
#define HCR_AMO_BIT	(1u << 5)
#define HCR_IMO_BIT	(1u << 4)
#define HCR_FMO_BIT	(1u << 3)
#define HCR_PTW_BIT	(1u << 2)
#define HCR_SWIO_BIT	(1u << 1)
#define HCR_VM_BIT	(1u << 0)

/* exception class */
#define ESR_EC_SHIFT		(26)
#define ESR_EC(esr)		GET_FIELD((esr), 31, ESR_EC_SHIFT)
/* instruction length */
#define ESR_IL(esr)		GET_FIELD((esr), 25, 25)
/* Instruction specific syndrome */
#define ESR_ISS(esr)		GET_FIELD((esr), 24, 0)
/* Exception classes values */
#define ESR_EC_UNKNOWN		0x00
#define ESR_EC_WFx		0x01
#define ESR_EC_CP15_32		0x03
#define ESR_EC_CP15_64		0x04
#define ESR_EC_CP14_MR		0x05
#define ESR_EC_CP14_LS		0x06
#define ESR_EC_FP_ASIMD		0x07
#define ESR_EC_CP10_ID		0x08
#define ESR_EC_CP14_64		0x0C
#define ESR_EC_ILL		0x0E
#define ESR_EC_SVC32		0x11
#define ESR_EC_HVC32		0x12
#define ESR_EC_SMC32		0x13
#define ESR_EC_SVC64		0x15
#define ESR_EC_HVC64		0x16
#define ESR_EC_SMC64		0x17
#define ESR_EC_SYS64		0x18
#define ESR_EC_IMP_DEF		0x1f
#define ESR_EC_IABT_LOW		0x20
#define ESR_EC_IABT_CUR		0x21
#define ESR_EC_PC_ALIGN		0x22
#define ESR_EC_DABT_LOW		0x24
#define ESR_EC_DABT_CUR		0x25
#define ESR_EC_SP_ALIGN		0x26
#define ESR_EC_FP_EXC32		0x28
#define ESR_EC_FP_EXC64		0x2C
#define ESR_EC_SERROR		0x2F
#define ESR_EC_BREAKPT_LOW	0x30
#define ESR_EC_BREAKPT_CUR	0x31
#define ESR_EC_SOFTSTP_LOW	0x32
#define ESR_EC_SOFTSTP_CUR	0x33
#define ESR_EC_WATCHPT_LOW	0x34
#define ESR_EC_WATCHPT_CUR	0x35
#define ESR_EC_BKPT32		0x38
#define ESR_EC_VECTOR32		0x3A
#define ESR_EC_BRK64		0x3C

#define ESR_MATCH_MSR_MRS(esr, op0, op1, crn, crm, op2)		\
	(((esr) & (BIT_MASK(21, 10) | BIT_MASK(4, 1))) ==	\
	 (((op0) << 20) | ((op2) << 17) | ((op1) << 14) |	\
	  ((crn) << 10) | ((crm) << 1)))

/* exception level in SPSR_ELx */
#define SPSR_EL(spsr)		(((spsr) & 0xc) >> 2)

#define CPACR_EL1_FPEN_ALL	(3UL << 20)

#define FPEXC_EL2_EN_BIT	(1UL << 30)

#ifndef __ASSEMBLY__

#include <jailhouse/string.h>

#define SYSREG_32(op1, crn, crm, op2)	s3_##op1 ##_##crn ##_##crm ##_##op2
#define SYSREG_64(op1, crn, crm, op2)	SYSREG_32(op1, crn, crm, op2)

#define arm_write_sysreg(sysreg, val) \
	asm volatile ("msr	"__stringify(sysreg)", %0\n" \
			: : "r" ((u64)(val)))

#define arm_read_sysreg(sysreg, val) \
	asm volatile ("mrs	%0,  "__stringify(sysreg)"\n" : "=r" ((val)))

#endif /* __ASSEMBLY__ */

#endif
