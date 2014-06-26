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

#ifndef _JAILHOUSE_ASM_IRQCHIP_H
#define _JAILHOUSE_ASM_IRQCHIP_H

#include <asm/percpu.h>

#ifndef __ASSEMBLY__

struct sgi {
	/*
	 * Routing mode values:
	 * 0: use aff3.aff2.aff1.targets
	 * 1: all processors in the cell except this CPU
	 * 2: only this CPU
	 */
	u8	routing_mode;
	/* GICv2 only uses 8bit in targets, and no affinity routing */
	u8	aff1;
	u8	aff2;
	/* Only available on 64-bit, when CTLR.A3V is 1 */
	u8	aff3;
	u16	targets;
	u16	id;
};

struct irqchip_ops {
	int	(*init)(void);
	int	(*cpu_init)(struct per_cpu *cpu_data);

	int	(*send_sgi)(struct sgi *sgi);
	void	(*handle_irq)(struct per_cpu *cpu_data);
};

int irqchip_init(void);
int irqchip_cpu_init(struct per_cpu *cpu_data);

int irqchip_send_sgi(struct sgi *sgi);
void irqchip_handle_irq(struct per_cpu *cpu_data);

#endif /* __ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_IRQCHIP_H */
