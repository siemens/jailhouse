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

#define MAX_PENDING_IRQS	256

#include <jailhouse/cell.h>
#include <jailhouse/mmio.h>

#ifndef __ASSEMBLY__

struct per_cpu;

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
	void	(*cpu_reset)(struct per_cpu *cpu_data);
	void	(*cpu_shutdown)(struct per_cpu *cpu_data);
	int	(*cell_init)(struct cell *cell);
	void	(*cell_exit)(struct cell *cell);
	void	(*adjust_irq_target)(struct cell *cell, u16 irq_id);

	int	(*send_sgi)(struct sgi *sgi);
	void	(*eoi_irq)(u32 irqn, bool deactivate);
	int	(*inject_irq)(struct per_cpu *cpu_data, u16 irq_id);
	void	(*enable_maint_irq)(bool enable);
	bool	(*has_pending_irqs)(void);

	enum mmio_result (*handle_irq_target)(struct mmio_access *mmio,
					      unsigned int irq);
};

unsigned int irqchip_mmio_count_regions(struct cell *cell);

int irqchip_init(void);
int irqchip_cpu_init(struct per_cpu *cpu_data);
void irqchip_cpu_reset(struct per_cpu *cpu_data);
void irqchip_cpu_shutdown(struct per_cpu *cpu_data);

int irqchip_cell_init(struct cell *cell);
void irqchip_cell_reset(struct cell *cell);
void irqchip_cell_exit(struct cell *cell);

void irqchip_config_commit(struct cell *cell_added_removed);

int irqchip_send_sgi(struct sgi *sgi);
void irqchip_handle_irq(struct per_cpu *cpu_data);
void irqchip_eoi_irq(u32 irqn, bool deactivate);

bool irqchip_has_pending_irqs(void);

void irqchip_inject_pending(struct per_cpu *cpu_data);
void irqchip_set_pending(struct per_cpu *cpu_data, u16 irq_id);

bool irqchip_irq_in_cell(struct cell *cell, unsigned int irq_id);

#endif /* __ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_IRQCHIP_H */
