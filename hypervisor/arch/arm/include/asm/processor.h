/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_PROCESSOR_H
#define _JAILHOUSE_ASM_PROCESSOR_H

#include <jailhouse/types.h>
#include <jailhouse/utils.h>

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

#define MPIDR_CPUID_MASK	0x00ffffff
#define MPIDR_MP_BIT		(1 << 31)
#define MPIDR_U_BIT		(1 << 30)

#define PFR1_VIRT(pfr)		((pfr) >> 12 & 0xf)

#define SCTLR_M_BIT	(1 << 0)
#define SCTLR_A_BIT	(1 << 1)
#define SCTLR_C_BIT	(1 << 2)
#define SCTLR_CP15B_BIT (1 << 5)
#define SCTLR_ITD_BIT	(1 << 7)
#define SCTLR_SED_BIT	(1 << 8)
#define SCTLR_I_BIT	(1 << 12)
#define SCTLR_V_BIT	(1 << 13)
#define SCTLR_nTWI	(1 << 16)
#define SCTLR_nTWE	(1 << 18)
#define SCTLR_WXN_BIT	(1 << 19)
#define SCTLR_UWXN_BIT	(1 << 20)
#define SCTLR_FI_BIT	(1 << 21)
#define SCTLR_EE_BIT	(1 << 25)
#define SCTLR_TRE_BIT	(1 << 28)
#define SCTLR_AFE_BIT	(1 << 29)
#define SCTLR_TE_BIT	(1 << 30)

/* Bits to wipe on cell reset */
#define SCTLR_MASK	(SCTLR_M_BIT | SCTLR_A_BIT | SCTLR_C_BIT	\
			| SCTLR_I_BIT | SCTLR_V_BIT | SCTLR_WXN_BIT	\
			| SCTLR_UWXN_BIT | SCTLR_FI_BIT | SCTLR_EE_BIT	\
			| SCTLR_TRE_BIT | SCTLR_AFE_BIT | SCTLR_TE_BIT)

#define HCR_TRVM_BIT	(1 << 30)
#define HCR_TVM_BIT	(1 << 26)
#define HCR_HDC_BIT	(1 << 29)
#define HCR_TGE_BIT	(1 << 27)
#define HCR_TTLB_BIT	(1 << 25)
#define HCR_TPU_BIT	(1 << 24)
#define HCR_TPC_BIT	(1 << 23)
#define HCR_TSW_BIT	(1 << 22)
#define HCR_TAC_BIT	(1 << 21)
#define HCR_TIDCP_BIT	(1 << 20)
#define HCR_TSC_BIT	(1 << 19)
#define HCR_TID3_BIT	(1 << 18)
#define HCR_TID2_BIT	(1 << 17)
#define HCR_TID1_BIT	(1 << 16)
#define HCR_TID0_BIT	(1 << 15)
#define HCR_TWE_BIT	(1 << 14)
#define HCR_TWI_BIT	(1 << 13)
#define HCR_DC_BIT	(1 << 12)
#define HCR_BSU_BITS	(3 << 10)
#define HCR_BSU_INNER	(1 << 10)
#define HCR_BSU_OUTER	(2 << 10)
#define HCR_BSU_FULL	HCR_BSU_BITS
#define HCR_FB_BIT	(1 << 9)
#define HCR_VA_BIT	(1 << 8)
#define HCR_VI_BIT	(1 << 7)
#define HCR_VF_BIT	(1 << 6)
#define HCR_AMO_BIT	(1 << 5)
#define HCR_IMO_BIT	(1 << 4)
#define HCR_FMO_BIT	(1 << 3)
#define HCR_PTW_BIT	(1 << 2)
#define HCR_SWIO_BIT	(1 << 1)
#define HCR_VM_BIT	(1 << 0)

#define PAR_F_BIT	0x1
#define PAR_FST_SHIFT	1
#define PAR_FST_MASK	0x3f
#define PAR_SHA_SHIFT	7
#define PAR_SHA_MASK	0x3
#define PAR_NS_BIT	(0x1 << 9)
#define PAR_LPAE_BIT	(0x1 << 11)
#define PAR_PA_MASK	BIT_MASK(39, 12)
#define PAR_ATTR_SHIFT	56
#define PAR_ATTR_MASK	0xff

/* exception class */
#define ESR_EC_SHIFT		26
#define ESR_EC(hsr)		((hsr) >> ESR_EC_SHIFT & 0x3f)
/* instruction length */
#define ESR_IL_SHIFT		25
#define ESR_IL(hsr)		((hsr) >> ESR_IL_SHIFT & 0x1)
/* Instruction specific */
#define ESR_ICC_MASK		0x1ffffff
#define ESR_ICC(hsr)		((hsr) & ESR_ICC_MASK)
/* Exception classes values */
#define ESR_EC_UNK		0x00
#define ESR_EC_WFI		0x01
#define ESR_EC_CP15_32		0x03
#define ESR_EC_CP15_64		0x04
#define ESR_EC_CP14_32		0x05
#define ESR_EC_CP14_LC		0x06
#define ESR_EC_HCPTR		0x07
#define ESR_EC_CP10		0x08
#define ESR_EC_CP14_64		0x0c
#define ESR_EC_SVC_HYP		0x11
#define ESR_EC_HVC		0x12
#define ESR_EC_SMC		0x13
#define ESR_EC_IABT		0x20
#define ESR_EC_IABT_HYP		0x21
#define ESR_EC_PCALIGN		0x22
#define ESR_EC_DABT		0x24
#define ESR_EC_DABT_HYP		0x25
/* Condition code */
#define ESR_ICC_CV_BIT		(1 << 24)
#define ESR_ICC_COND(icc)	((icc) >> 20 & 0xf)

#define EXIT_REASON_UNDEF	0x1
#define EXIT_REASON_HVC		0x2
#define EXIT_REASON_PABT	0x3
#define EXIT_REASON_DABT	0x4
#define EXIT_REASON_TRAP	0x5
#define EXIT_REASON_IRQ		0x6
#define EXIT_REASON_FIQ		0x7

#define NUM_USR_REGS		14

#ifndef __ASSEMBLY__

struct registers {
	unsigned long exit_reason;
	/* r0 - r12 and lr. The other registers are banked. */
	unsigned long usr[NUM_USR_REGS];
};

#define dmb(domain)	asm volatile("dmb " #domain ::: "memory")
#define dsb(domain)	asm volatile("dsb " #domain ::: "memory")
#define isb()		asm volatile("isb")

#define wfe()		asm volatile("wfe")
#define wfi()		asm volatile("wfi")
#define sev()		asm volatile("sev")

unsigned int smc(unsigned int r0, ...);
unsigned int hvc(unsigned int r0, ...);

static inline void cpu_relax(void)
{
	asm volatile("" : : : "memory");
}

static inline void memory_barrier(void)
{
	dmb(ish);
}

static inline void memory_load_barrier(void)
{
}

static inline bool is_el2(void)
{
	u32 psr;

	asm volatile ("mrs	%0, cpsr" : "=r" (psr));

	return (psr & PSR_MODE_MASK) == PSR_HYP_MODE;
}

#define tlb_flush_guest()	arm_write_sysreg(TLBIALL, 1)

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PROCESSOR_H */
