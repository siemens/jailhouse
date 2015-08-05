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

#ifndef _JAILHOUSE_ASM_GIC_COMMON_H
#define _JAILHOUSE_ASM_GIC_COMMON_H

#include <jailhouse/mmio.h>
#include <jailhouse/types.h>

#define GICD_CTLR			0x0000
#define GICD_TYPER			0x0004
#define GICD_IIDR			0x0008
#define GICD_IGROUPR			0x0080
#define GICD_ISENABLER			0x0100
#define GICD_ICENABLER			0x0180
#define GICD_ISPENDR			0x0200
#define GICD_ICPENDR			0x0280
#define GICD_ISACTIVER			0x0300
#define GICD_ICACTIVER			0x0380
#define GICD_IPRIORITYR			0x0400
#define GICD_ITARGETSR			0x0800
#define GICD_ICFGR			0x0c00
#define GICD_NSACR			0x0e00
#define GICD_SGIR			0x0f00
#define GICD_CPENDSGIR			0x0f10
#define GICD_SPENDSGIR			0x0f20
#define GICD_IROUTER			0x6000

#define GICD_PIDR2_ARCH(pidr)		(((pidr) & 0xf0) >> 4)

#define is_sgi(irqn)			((u32)(irqn) < 16)
#define is_ppi(irqn)			((irqn) > 15 && (irqn) < 32)
#define is_spi(irqn)			((irqn) > 31 && (irqn) < 1020)

#ifndef __ASSEMBLY__

struct cell;
struct arm_mmio_access;
struct per_cpu;
struct sgi;

int gic_probe_cpu_id(unsigned int cpu);
int gic_handle_dist_access(struct mmio_access *access);
void gic_handle_sgir_write(struct sgi *sgi, bool virt_input);
void gic_handle_irq(struct per_cpu *cpu_data);
void gic_target_spis(struct cell *config_cell, struct cell *dest_cell);

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_GIC_COMMON_H */
