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

#include <mach.h>
#include <gic.h>

#define GICC_CTLR		0x0000
#define GICC_PMR		0x0004
#define GICC_IAR		0x000c
#define GICC_EOIR		0x0010
#define GICD_CTLR		0x0000
#define  GICD_CTLR_ENABLE	(1 << 0)

#define GICC_CTLR_GRPEN1	(1 << 0)

#define GICC_PMR_DEFAULT	0xf0

void gic_enable(unsigned int irqn)
{
	mmio_write32(GICD_V2_BASE + GICD_ISENABLER + ((irqn >> 3) & ~0x3),
		     1 << (irqn & 0x1f));
}

int gic_init(void)
{
	mmio_write32(GICC_V2_BASE + GICC_CTLR, GICC_CTLR_GRPEN1);
	mmio_write32(GICC_V2_BASE + GICC_PMR, GICC_PMR_DEFAULT);
	mmio_write32(GICD_V2_BASE + GICD_CTLR, GICD_CTLR_ENABLE);

	return 0;
}

void gic_write_eoi(u32 irqn)
{
	mmio_write32(GICC_V2_BASE + GICC_EOIR, irqn);
}

u32 gic_read_ack(void)
{
	return mmio_read32(GICC_V2_BASE + GICC_IAR);
}
