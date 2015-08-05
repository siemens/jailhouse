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

/*
 * Since there is no finer-grained allocation than page-alloc for the moment,
 * and it is very complicated to predict the total size needed at
 * initialisation, each cpu is allocated one page of pending irqs.
 * This allows for 256 pending IRQs, which should be sufficient.
 */
#define MAX_PENDING_IRQS	(PAGE_SIZE / sizeof(struct pending_irq))

#include <jailhouse/mmio.h>
#include <asm/percpu.h>
#include <asm/traps.h>

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
	int	(*cell_init)(struct cell *cell);
	void	(*cell_exit)(struct cell *cell);
	int	(*cpu_reset)(struct per_cpu *cpu_data, bool is_shutdown);

	int	(*send_sgi)(struct sgi *sgi);
	void	(*handle_irq)(struct per_cpu *cpu_data);
	void	(*eoi_irq)(u32 irqn, bool deactivate);
	int	(*inject_irq)(struct per_cpu *cpu_data,
			      struct pending_irq *irq);

	int	(*mmio_access)(struct mmio_access *access);
};

/* Virtual interrupts waiting to be injected */
struct pending_irq {
	u32	virt_id;

	u8	priority;
	u8	hw;
	union {
		/* Physical id, when hw is 1 */
		u16 irq;
		struct {
			/* GICv2 needs cpuid for SGIs */
			u16 cpuid	: 15;
			/* EOI generates a maintenance irq */
			u16 maintenance	: 1;
		} sgi __attribute__((packed));
	} type;

	struct pending_irq *next;
	struct pending_irq *prev;
} __attribute__((packed));

unsigned int irqchip_mmio_count_regions(struct cell *cell);

int irqchip_init(void);
int irqchip_cpu_init(struct per_cpu *cpu_data);
int irqchip_cpu_reset(struct per_cpu *cpu_data);
void irqchip_cpu_shutdown(struct per_cpu *cpu_data);

int irqchip_cell_init(struct cell *cell);
void irqchip_cell_exit(struct cell *cell);
void irqchip_root_cell_shrink(struct cell *cell);

int irqchip_send_sgi(struct sgi *sgi);
void irqchip_handle_irq(struct per_cpu *cpu_data);
void irqchip_eoi_irq(u32 irqn, bool deactivate);

int irqchip_inject_pending(struct per_cpu *cpu_data);
int irqchip_insert_pending(struct per_cpu *cpu_data, struct pending_irq *irq);
int irqchip_remove_pending(struct per_cpu *cpu_data, struct pending_irq *irq);
int irqchip_set_pending(struct per_cpu *cpu_data, u32 irq_id, bool try_inject);

static inline bool spi_in_cell(struct cell *cell, unsigned int spi)
{
	/* FIXME: Change the configuration to a bitmask range */
	u32 spi_mask;

	if (spi >= 64)
		return false;
	else if (spi >= 32)
		spi_mask = cell->arch.spis >> 32;
	else
		spi_mask = cell->arch.spis;

	return spi_mask & (1 << (spi & 31));
}

#endif /* __ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_IRQCHIP_H */
