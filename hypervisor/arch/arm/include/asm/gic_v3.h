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

#ifndef _JAILHOUSE_ASM_GIC_V3_H
#define _JAILHOUSE_ASM_GIC_V3_H

#include <asm/sysregs.h>

#define GICD_CIDR0		0xfff0
#define GICD_CIDR1		0xfff4
#define GICD_CIDR2		0xfff8
#define GICD_CIDR3		0xfffc

#define GICD_PIDR0		0xffe0
#define GICD_PIDR1		0xffe4
#define GICD_PIDR2		0xffe8
#define GICD_PIDR3		0xffec
#define GICD_PIDR4		0xffd0
#define GICD_PIDR5		0xffd4
#define GICD_PIDR6		0xffd8
#define GICD_PIDR7		0xffdc

#define GICR_CTLR		GICD_CTLR
#define GICR_TYPER		0x0008
#define GICR_WAKER		0x0014
#define GICR_CIDR0		GICD_CIDR0
#define GICR_CIDR1		GICD_CIDR1
#define GICR_CIDR2		GICD_CIDR2
#define GICR_CIDR3		GICD_CIDR3
#define GICR_PIDR0		GICD_PIDR0
#define GICR_PIDR1		GICD_PIDR1
#define GICR_PIDR2		GICD_PIDR2
#define GICR_PIDR3		GICD_PIDR3
#define GICR_PIDR4		GICD_PIDR4
#define GICR_PIDR5		GICD_PIDR5
#define GICR_PIDR6		GICD_PIDR6
#define GICR_PIDR7		GICD_PIDR7

#define GICR_SGI_BASE		0x10000
#define GICR_IGROUPR		GICD_IGROUPR
#define GICR_ISENABLER		GICD_ISENABLER
#define GICR_ICENABLER		GICD_ICENABLER
#define GICR_ISACTIVER		GICD_ISACTIVER
#define GICR_ICACTIVER		GICD_ICACTIVER
#define GICR_IPRIORITY		GICD_IPRIORITY

#define GICR_TYPER_Last		(1 << 4)
#define GICR_PIDR2_ARCH		GICD_PIDR2_ARCH

#define ICC_IAR1_EL1		SYSREG_32(0, c12, c12, 0)
#define ICC_EOIR1_EL1		SYSREG_32(0, c12, c12, 1)
#define ICC_HPPIR1_EL1		SYSREG_32(0, c12, c12, 2)
#define ICC_BPR1_EL1		SYSREG_32(0, c12, c12, 3)
#define ICC_DIR_EL1		SYSREG_32(0, c12, c11, 1)
#define ICC_PMR_EL1		SYSREG_32(0, c4, c6, 0)
#define ICC_RPR_EL1		SYSREG_32(0, c12, c11, 3)
#define ICC_CTLR_EL1		SYSREG_32(0, c12, c12, 4)
#define ICC_SRE_EL1		SYSREG_32(0, c12, c12, 5)
#define ICC_SRE_EL2		SYSREG_32(4, c12, c9, 5)
#define ICC_IGRPEN1_EL1		SYSREG_32(0, c12, c12, 7)
#define ICC_SGI1R_EL1		SYSREG_64(0, c12)
#define ICC_AP1R0_EL1		SYSREG_32(0, c12, c9, 0)
#define ICC_AP1R1_EL1		SYSREG_32(0, c12, c9, 1)
#define ICC_AP1R2_EL1		SYSREG_32(0, c12, c9, 2)
#define ICC_AP1R3_EL1		SYSREG_32(0, c12, c9, 3)

#define ICH_HCR_EL2		SYSREG_32(4, c12, c11, 0)
#define ICH_VTR_EL2		SYSREG_32(4, c12, c11, 1)
#define ICH_MISR_EL2		SYSREG_32(4, c12, c11, 2)
#define ICH_EISR_EL2		SYSREG_32(4, c12, c11, 3)
#define ICH_ELSR_EL2		SYSREG_32(4, c12, c11, 5)
#define ICH_VMCR_EL2		SYSREG_32(4, c12, c11, 7)
#define ICH_AP1R0_EL2		SYSREG_32(4, c12, c9, 0)
#define ICH_AP1R1_EL2		SYSREG_32(4, c12, c9, 1)
#define ICH_AP1R2_EL2		SYSREG_32(4, c12, c9, 2)
#define ICH_AP1R3_EL2		SYSREG_32(4, c12, c9, 3)

/* Different on AArch32 and AArch64... */
#define __ICH_LR0(x)		SYSREG_32(4, c12, c12, x)
#define __ICH_LR8(x)		SYSREG_32(4, c12, c13, x)
#define __ICH_LRC0(x)		SYSREG_32(4, c12, c14, x)
#define __ICH_LRC8(x)		SYSREG_32(4, c12, c15, x)

#define ICH_LR0			__ICH_LR0(0)
#define ICH_LR1			__ICH_LR0(1)
#define ICH_LR2			__ICH_LR0(2)
#define ICH_LR3			__ICH_LR0(3)
#define ICH_LR4			__ICH_LR0(4)
#define ICH_LR5			__ICH_LR0(5)
#define ICH_LR6			__ICH_LR0(6)
#define ICH_LR7			__ICH_LR0(7)
#define ICH_LR8			__ICH_LR8(0)
#define ICH_LR9			__ICH_LR8(1)
#define ICH_LR10		__ICH_LR8(2)
#define ICH_LR11		__ICH_LR8(3)
#define ICH_LR12		__ICH_LR8(4)
#define ICH_LR13		__ICH_LR8(5)
#define ICH_LR14		__ICH_LR8(6)
#define ICH_LR15		__ICH_LR8(7)
#define ICH_LRC0		__ICH_LRC0(0)
#define ICH_LRC1		__ICH_LRC0(1)
#define ICH_LRC2		__ICH_LRC0(2)
#define ICH_LRC3		__ICH_LRC0(3)
#define ICH_LRC4		__ICH_LRC0(4)
#define ICH_LRC5		__ICH_LRC0(5)
#define ICH_LRC6		__ICH_LRC0(6)
#define ICH_LRC7		__ICH_LRC0(7)
#define ICH_LRC8		__ICH_LRC8(0)
#define ICH_LRC9		__ICH_LRC8(1)
#define ICH_LRC10		__ICH_LRC8(2)
#define ICH_LRC11		__ICH_LRC8(3)
#define ICH_LRC12		__ICH_LRC8(4)
#define ICH_LRC13		__ICH_LRC8(5)
#define ICH_LRC14		__ICH_LRC8(6)
#define ICH_LRC15		__ICH_LRC8(7)

#define ICC_CTLR_EOImode	0x2
#define ICC_PMR_MASK		0xff
#define ICC_PMR_DEFAULT		0xf0
#define ICC_IGRPEN1_EN		0x1

#define ICC_SGIR_AFF3_SHIFT	48
#define ICC_SGIR_AFF2_SHIFT	32
#define ICC_SGIR_AFF1_SHIFT	16
#define ICC_SGIR_TARGET_MASK	0xffff
#define ICC_SGIR_IRQN_SHIFT	24
#define ICC_SGIR_ROUTING_BIT	(1ULL << 40)

#define ICH_HCR_EN		(1 << 0)
#define ICH_HCR_UIE		(1 << 1)
#define ICH_HCR_LRENPIE		(1 << 2)
#define ICH_HCR_NPIE		(1 << 3)
#define ICH_HCR_VGRP0EIE	(1 << 4)
#define ICH_HCR_VGRP0DIE	(1 << 5)
#define ICH_HCR_VGRP1EIE	(1 << 6)
#define ICH_HCR_VGRP1DIE	(1 << 7)
#define ICH_HCR_VARE		(1 << 9)
#define ICH_HCR_TC		(1 << 10)
#define ICH_HCR_TALL0		(1 << 11)
#define ICH_HCR_TALL1		(1 << 12)
#define ICH_HCR_TSEI		(1 << 13)
#define ICH_HCR_EOICount	(0x1f << 27)

#define ICH_MISR_EOI		(1 << 0)
#define ICH_MISR_U		(1 << 1)
#define ICH_MISR_LRENP		(1 << 2)
#define ICH_MISR_NP		(1 << 3)
#define ICH_MISR_VGRP0E		(1 << 4)
#define ICH_MISR_VGRP0D		(1 << 5)
#define ICH_MISR_VGRP1E		(1 << 6)
#define ICH_MISR_VGRP1D		(1 << 7)

#define ICH_VMCR_VENG0		(1 << 0)
#define ICH_VMCR_VENG1		(1 << 1)
#define ICH_VMCR_VACKCTL	(1 << 2)
#define ICH_VMCR_VFIQEN		(1 << 3)
#define ICH_VMCR_VCBPR		(1 << 4)
#define ICH_VMCR_VEOIM		(1 << 9)
#define ICH_VMCR_VBPR1_SHIFT	18
#define ICH_VMCR_VBPR0_SHIFT	21
#define ICH_VMCR_VPMR_SHIFT	24

/* List registers upper bits */
#define ICH_LR_INVALID		(0x0ULL << 62)
#define ICH_LR_PENDING		(0x1ULL << 62)
#define ICH_LR_ACTIVE		(0x2ULL << 62)
#define ICH_LR_PENDACTIVE	(0x3ULL << 62)
#define ICH_LR_HW_BIT		(0x1ULL << 61)
#define ICH_LR_GROUP_BIT	(0x1ULL << 60)
#define ICH_LR_PRIORITY_SHIFT	48
#define ICH_LR_SGI_EOI		(0x1ULL << 41)
#define ICH_LR_PHYS_ID_SHIFT	32

#ifndef __ASSEMBLY__

#include <jailhouse/types.h>

static inline u64 gic_read_lr(unsigned int n)
{
	u32 lr, lrc;

	switch (n) {
#define __READ_LR(n)					\
	case n:						\
		arm_read_sysreg(ICH_LR##n, lr);		\
		arm_read_sysreg(ICH_LRC##n, lrc);	\
		break;

	__READ_LR(0)
	__READ_LR(1)
	__READ_LR(2)
	__READ_LR(3)
	__READ_LR(4)
	__READ_LR(5)
	__READ_LR(6)
	__READ_LR(7)
	__READ_LR(8)
	__READ_LR(9)
	__READ_LR(10)
	__READ_LR(11)
	__READ_LR(12)
	__READ_LR(13)
	__READ_LR(14)
	__READ_LR(15)
#undef __READ_LR

	default:
		return (u64)(-1);
	}

	return (u64)lrc << 32 | lr;
}

static inline void gic_write_lr(unsigned int n, u64 val)
{
	u32 lr = (u32)val;
	u32 lrc = val >> 32;

	switch (n) {
#define __WRITE_LR(n)					\
	case n:						\
		arm_write_sysreg(ICH_LR##n, lr);	\
		arm_write_sysreg(ICH_LRC##n, lrc);	\
		break;

	__WRITE_LR(0)
	__WRITE_LR(1)
	__WRITE_LR(2)
	__WRITE_LR(3)
	__WRITE_LR(4)
	__WRITE_LR(5)
	__WRITE_LR(6)
	__WRITE_LR(7)
	__WRITE_LR(8)
	__WRITE_LR(9)
	__WRITE_LR(10)
	__WRITE_LR(11)
	__WRITE_LR(12)
	__WRITE_LR(13)
	__WRITE_LR(14)
	__WRITE_LR(15)
#undef __WRITE_LR
	}
}

static inline u32 gic_read_iar(void)
{
	u32 irq_id;

	arm_read_sysreg(ICC_IAR1_EL1, irq_id);
	return irq_id;
}

int gicv3_handle_sgir_write(u64 sgir);

#endif /* __ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_GIC_V3_H */
