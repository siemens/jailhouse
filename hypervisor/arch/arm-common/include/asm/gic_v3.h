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
#include <asm/arch_gicv3.h>

#define GICDv3_CIDR0		0xfff0
#define GICDv3_PIDR0		0xffe0
#define GICDv3_PIDR2		0xffe8
#define GICDv3_PIDR4		0xffd0

#define GICR_CTLR		0x0000
#define GICR_IIDR		0x0004
#define GICR_TYPER		0x0008
#define GICR_STATUSR		0x0010
#define GICR_WAKER		0x0014
#define GICR_SYNCR		0x00c0
#define GICR_PIDR2		0xffe8

#define GICR_SGI_BASE		0x10000
#define GICR_ISENABLER		GICD_ISENABLER
#define GICR_ICENABLER		GICD_ICENABLER
#define GICR_ISPENDR		GICD_ISPENDR
#define GICR_ICPENDR		GICD_ICPENDR
#define GICR_ISACTIVER		GICD_ISACTIVER
#define GICR_ICACTIVER		GICD_ICACTIVER
#define GICR_IPRIORITYR		GICD_IPRIORITYR
#define GICR_ICFGR		GICD_ICFGR

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
#endif /* _JAILHOUSE_ASM_GIC_V3_H */
