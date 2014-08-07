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
#include <asm/gic_v2.h>
#include <inmates/gic.h>
#include <inmates/inmate.h>
#include <mach/gic_v2.h>

void gic_enable(unsigned int irqn)
{
	mmio_write32(GICD_BASE + GICD_ISENABLER, 1 << irqn);
}

int gic_init(void)
{
	mmio_write32(GICC_BASE + GICC_CTLR, GICC_CTLR_GRPEN1);
	mmio_write32(GICC_BASE + GICC_PMR, GICC_PMR_DEFAULT);

	return 0;
}

void gic_write_eoi(u32 irqn)
{
	mmio_write32(GICC_BASE + GICC_EOIR, irqn);
}

u32 gic_read_ack(void)
{
	return mmio_read32(GICC_BASE + GICC_IAR);
}
