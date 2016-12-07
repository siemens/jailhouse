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

#include <asm/sysregs.h>
#include <mach.h>
#include <gic.h>

#define GICR_SGI_BASE		0x10000
#define GICR_ISENABLER		GICD_ISENABLER

#define ICC_IAR1_EL1		SYSREG_32(0, c12, c12, 0)
#define ICC_EOIR1_EL1		SYSREG_32(0, c12, c12, 1)
#define ICC_PMR_EL1		SYSREG_32(0, c4, c6, 0)
#define ICC_CTLR_EL1		SYSREG_32(0, c12, c12, 4)
#define ICC_IGRPEN1_EL1		SYSREG_32(0, c12, c12, 7)

#define ICC_IGRPEN1_EN		0x1

void gic_enable(unsigned int irqn)
{
	if (is_sgi_ppi(irqn))
		mmio_write32(GICR_V3_BASE + GICR_SGI_BASE + GICR_ISENABLER,
			     1 << irqn);
	else if (is_spi(irqn))
		mmio_write32(GICD_V3_BASE + GICD_ISENABLER + irqn / 32,
			     1 << (irqn % 32));
}

int gic_init(void)
{
	arm_write_sysreg(ICC_CTLR_EL1, 0);
	arm_write_sysreg(ICC_PMR_EL1, 0xf0);
	arm_write_sysreg(ICC_IGRPEN1_EL1, ICC_IGRPEN1_EN);

	return 0;
}

void gic_write_eoi(u32 irqn)
{
	arm_write_sysreg(ICC_EOIR1_EL1, irqn);
}

u32 gic_read_ack(void)
{
	u32 val;

	arm_read_sysreg(ICC_IAR1_EL1, val);
	return val;
}
