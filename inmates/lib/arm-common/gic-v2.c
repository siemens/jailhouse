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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <gic.h>

#define GICC_CTLR		0x0000
#define GICC_PMR		0x0004
#define GICC_IAR		0x000c
#define GICC_EOIR		0x0010
#define GICD_CTLR		0x0000
#define  GICD_CTLR_ENABLE	(1 << 0)

#define GICC_CTLR_GRPEN1	(1 << 0)

#define GICC_PMR_DEFAULT	0xf0

static void *gicc_v2_base;
static void *gicd_v2_base;

static void gic_v2_enable(unsigned int irqn)
{
	mmio_write32(gicd_v2_base + GICD_ISENABLER + ((irqn >> 3) & ~0x3),
		     1 << (irqn & 0x1f));
}

static int gic_v2_init(void)
{
	gicc_v2_base = (void*)(unsigned long)comm_region->gicc_base;
	gicd_v2_base = (void*)(unsigned long)comm_region->gicd_base;

	map_range(gicc_v2_base, PAGE_SIZE, MAP_UNCACHED);
	map_range(gicd_v2_base, PAGE_SIZE, MAP_UNCACHED);

	mmio_write32(gicc_v2_base + GICC_CTLR, GICC_CTLR_GRPEN1);
	mmio_write32(gicc_v2_base + GICC_PMR, GICC_PMR_DEFAULT);
	mmio_write32(gicd_v2_base + GICD_CTLR, GICD_CTLR_ENABLE);

	return 0;
}

static void gic_v2_write_eoi(u32 irqn)
{
	mmio_write32(gicc_v2_base + GICC_EOIR, irqn);
}

static u32 gic_v2_read_ack(void)
{
	return mmio_read32(gicc_v2_base + GICC_IAR) & 0x3ff;
}

const struct gic gic_v2 = {
	.init = gic_v2_init,
	.enable = gic_v2_enable,
	.write_eoi = gic_v2_write_eoi,
	.read_ack = gic_v2_read_ack,
};
