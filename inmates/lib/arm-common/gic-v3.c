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

#include <asm/sysregs.h>
#include <gic.h>

static void *gicd_v3_base;
static void *gicr_v3_base;

#define GICR_TYPER              0x0008
#define GICR_TYPER_Last         (1 << 4)
#define GICR_PIDR2              0xffe8
#define GICR_SGI_BASE		0x10000
#define GICR_ISENABLER		GICD_ISENABLER

#define GICD_PIDR2_ARCH(pidr)	(((pidr) & 0xf0) >> 4)
#define GICR_PIDR2_ARCH		GICD_PIDR2_ARCH

static void gic_v3_enable(unsigned int irqn)
{
	if (is_sgi_ppi(irqn))
		mmio_write32(gicr_v3_base + GICR_SGI_BASE + GICR_ISENABLER,
			     1 << irqn);
	else if (is_spi(irqn))
		mmio_write32(gicd_v3_base + GICD_ISENABLER + (irqn / 32) * 4,
			     1 << (irqn % 32));
}

static int gic_v3_init(void)
{
	unsigned long mpidr;
	void *redist_addr;
	void *gicr = NULL;
	u64 typer;
	u32 pidr, aff;

	redist_addr = (void*)(unsigned long)comm_region->gicr_base;
	gicd_v3_base = (void*)(unsigned long)comm_region->gicd_base;

	map_range(gicd_v3_base, PAGE_SIZE, MAP_UNCACHED);
	map_range(redist_addr, PAGE_SIZE, MAP_UNCACHED);

	arm_read_sysreg(MPIDR, mpidr);
	mpidr &= MPIDR_CPUID_MASK;
	aff = (MPIDR_AFFINITY_LEVEL(mpidr, 3) << 24 |
		MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
		MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8 |
		MPIDR_AFFINITY_LEVEL(mpidr, 0));

	/* Find redistributor */
	do {
		pidr = mmio_read32(redist_addr + GICR_PIDR2);
		if (GICR_PIDR2_ARCH(pidr) != 3)
			break;

		typer = mmio_read32(redist_addr + GICR_TYPER);
		typer |= (u64)mmio_read32(redist_addr + GICR_TYPER + 4) << 32;
		if ((typer >> 32) == aff) {
			gicr = redist_addr;
			break;
		}

		redist_addr += 0x20000;
	} while (!(typer & GICR_TYPER_Last));

	if (!gicr)
		return -1;

	gicr_v3_base = gicr;

	arm_write_sysreg(ICC_CTLR_EL1, 0);
	arm_write_sysreg(ICC_PMR_EL1, 0xf0);
	arm_write_sysreg(ICC_IGRPEN1_EL1, ICC_IGRPEN1_EN);

	return 0;
}

static void gic_v3_write_eoi(u32 irqn)
{
	arm_write_sysreg(ICC_EOIR1_EL1, irqn);
}

static u32 gic_v3_read_ack(void)
{
	u32 val;

	arm_read_sysreg(ICC_IAR1_EL1, val);
	return val & 0xffffff;
}

const struct gic gic_v3 = {
	.init = gic_v3_init,
	.enable = gic_v3_enable,
	.write_eoi = gic_v3_write_eoi,
	.read_ack = gic_v3_read_ack,
};
