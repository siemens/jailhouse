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

#ifndef _JAILHOUSE_ASM_GIC_V2_H
#define _JAILHOUSE_ASM_GIC_V2_H

#define GICC_SIZE		0x2000
#define GICD_SIZE		0x1000
#define GICH_SIZE		0x2000

#define GICD_CIDR0		0xff0
#define GICD_CIDR1		0xff4
#define GICD_CIDR2		0xff8
#define GICD_CIDR3		0xffc

#define GICD_PIDR0		0xfe0
#define GICD_PIDR1		0xfe4
#define GICD_PIDR2		0xfe8
#define GICD_PIDR3		0xfec
#define GICD_PIDR4		0xfd0
#define GICD_PIDR5		0xfd4
#define GICD_PIDR6		0xfd8
#define GICD_PIDR7		0xfdc

#define GICC_CTLR		0x0000
#define GICC_PMR		0x0004
#define GICC_BPR		0x0008
#define GICC_IAR		0x000c
#define GICC_EOIR		0x0010
#define GICC_RPR		0x0014
#define GICC_HPPIR		0x0018
#define GICC_ABPR		0x001c
#define GICC_AIAR		0x0020
#define GICC_AEOIR		0x0024
#define GICC_AHPPIR		0x0028
#define GICC_APR0		0x00d0
#define GICC_APR1		0x00d4
#define GICC_APR2		0x00d8
#define GICC_APR3		0x00dc
#define GICC_NSAPR0		0x00e0
#define GICC_NSAPR1		0x00e4
#define GICC_NSAPR2		0x00e8
#define GICC_NSAPR3		0x00ec
#define GICC_IIDR		0x00fc
#define GICC_DIR		0x1000

#define GICC_CTLR_GRPEN1	(1 << 0)
#define GICC_CTLR_EOImode	(1 << 9)

#define GICC_PMR_DEFAULT	0xf0

#define GICH_HCR		0x000
#define GICH_VTR		0x004
#define GICH_VMCR		0x008
#define GICH_MISR		0x010
#define GICH_EISR0		0x020
#define GICH_EISR1		0x024
#define GICH_ELSR0		0x030
#define GICH_ELSR1		0x034
#define GICH_APR		0x0f0
#define GICH_LR_BASE		0x100

#define GICV_PMR_SHIFT		3
#define GICH_VMCR_PMR_SHIFT	27
#define GICH_VMCR_EN0		(1 << 0)
#define GICH_VMCR_EN1		(1 << 1)
#define GICH_VMCR_ACKCtl	(1 << 2)
#define GICH_VMCR_EOImode	(1 << 9)

#define GICH_HCR_EN		(1 << 0)
#define GICH_HCR_UIE		(1 << 1)
#define GICH_HCR_LRENPIE	(1 << 2)
#define GICH_HCR_NPIE		(1 << 3)
#define GICH_HCR_VGRP0EIE	(1 << 4)
#define GICH_HCR_VGRP0DIE	(1 << 5)
#define GICH_HCR_VGRP1EIE	(1 << 6)
#define GICH_HCR_VGRP1DIE	(1 << 7)
#define GICH_HCR_EOICOUNT_SHIFT	27

#define GICH_LR_HW_BIT		(1 << 31)
#define GICH_LR_GRP1_BIT	(1 << 30)
#define GICH_LR_ACTIVE_BIT	(1 << 29)
#define GICH_LR_PENDING_BIT	(1 << 28)
#define GICH_LR_PRIORITY_SHIFT	23
#define GICH_LR_SGI_EOI_BIT	(1 << 19)
#define GICH_LR_CPUID_SHIFT	10
#define GICH_LR_PHYS_ID_SHIFT	10
#define GICH_LR_VIRT_ID_MASK	0x3ff

#ifndef __ASSEMBLY__

#include <jailhouse/mmio.h>

static inline u32 gic_read_lr(unsigned int i)
{
	extern void *gich_base;

	return mmio_read32(gich_base + GICH_LR_BASE + i * 4);
}

static inline void gic_write_lr(unsigned int i, u32 value)
{
	extern void *gich_base;

	mmio_write32(gich_base + GICH_LR_BASE + i * 4, value);
}

static inline u32 gic_read_iar(void)
{
	extern void *gicc_base;

	return mmio_read32(gicc_base + GICC_IAR) & 0x3ff;
}

#endif /* !__ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_GIC_V2_H */
