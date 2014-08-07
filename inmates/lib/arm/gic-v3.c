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
#include <asm/gic_common.h>
#include <asm/gic_v3.h>
#include <inmates/gic.h>
#include <mach/gic_v3.h>

void gic_enable(unsigned int irqn)
{
	if (is_spi(irqn))
		mmio_write32(GICD_BASE + GICD_ISENABLER, 1 << irqn);
	else
		mmio_write32(GICR_BASE + GICR_SGI_BASE + GICR_ISENABLER,
			     1 << irqn);
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
