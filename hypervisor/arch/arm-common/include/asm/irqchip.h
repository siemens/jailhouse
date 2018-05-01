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
struct public_per_cpu;

struct sgi {
	/*
	 * Routing mode values:
	 * 0: use aff3.aff2.aff1.targets
	 * 1: all processors in the cell except this CPU
	 * 2: only this CPU
	 */
	u8	routing_mode;
	/* cluster_id: mpidr & MPIDR_CLUSTERID_MASK */
	u64	cluster_id;
	u16	targets;
	u16	id;
};

struct irqchip {
	int	(*init)(void);
	int	(*cpu_init)(struct per_cpu *cpu_data);
	void	(*cpu_reset)(struct per_cpu *cpu_data);
	int	(*cpu_shutdown)(struct public_per_cpu *cpu_public);
	int	(*cell_init)(struct cell *cell);
	void	(*cell_exit)(struct cell *cell);
	void	(*adjust_irq_target)(struct cell *cell, u16 irq_id);

	int	(*send_sgi)(struct sgi *sgi);
	u32	(*read_iar_irqn)(void);
	void	(*eoi_irq)(u32 irqn, bool deactivate);
	int	(*inject_irq)(u16 irq_id, u16 sender);
	void	(*enable_maint_irq)(bool enable);
	bool	(*has_pending_irqs)(void);
	int	(*get_pending_irq)(void);
	void	(*inject_phys_irq)(u16 irq_id);

	int 	(*get_cpu_target)(unsigned int cpu_id);
	u64 	(*get_cluster_target)(unsigned int cpu_id);

	enum mmio_result (*handle_irq_route)(struct mmio_access *mmio,
					     unsigned int irq);
	enum mmio_result (*handle_irq_target)(struct mmio_access *mmio,
					      unsigned int irq);
	enum mmio_result (*handle_dist_access)(struct mmio_access *mmio);

	unsigned long gicd_size;
};

struct pending_irqs {
	/* synchronizes parallel insertions of SGIs into the pending ring */
	spinlock_t lock;
	u16 irqs[MAX_PENDING_IRQS];
	/* contains the calling CPU ID in case of a SGI */
	u16 sender[MAX_PENDING_IRQS];
	unsigned int head;
	/* removal from the ring happens lockless, thus tail is volatile */
	volatile unsigned int tail;
};

int irqchip_cpu_init(struct per_cpu *cpu_data);
int irqchip_get_cpu_target(unsigned int cpu_id);
u64 irqchip_get_cluster_target(unsigned int cpu_id);
void irqchip_cpu_reset(struct per_cpu *cpu_data);

void irqchip_cpu_shutdown(struct public_per_cpu *cpu_public);

void irqchip_cell_reset(struct cell *cell);

void irqchip_config_commit(struct cell *cell_added_removed);

int irqchip_send_sgi(struct sgi *sgi);
void irqchip_handle_irq(void);

bool irqchip_has_pending_irqs(void);

void irqchip_inject_pending(void);
void irqchip_set_pending(struct public_per_cpu *cpu_public, u16 irq_id);

bool irqchip_irq_in_cell(struct cell *cell, unsigned int irq_id);

#endif /* __ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_IRQCHIP_H */
