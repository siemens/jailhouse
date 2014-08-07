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

#include <inmates/inmate.h>
#include <inmates/gic.h>

static irq_handler_t irq_handler = (irq_handler_t)NULL;
static __attribute__((aligned(0x1000))) u32 irq_stack[1024];

/* Replaces the weak reference in header.S */
void vector_irq(void)
{
	u32 irqn;

	do {
		irqn = gic_read_ack();

		if (irq_handler)
			irq_handler(irqn);

		gic_write_eoi(irqn);

	} while (irqn != 0x3ff);
}

void gic_setup(irq_handler_t handler)
{
	gic_init();
	irq_handler = handler;

	asm volatile (".arch_extension virt\n");
	asm volatile ("msr	SP_irq, %0\n" : : "r" (irq_stack));
	asm volatile ("cpsie	i\n");
}

void gic_enable_irq(unsigned int irq)
{
	gic_enable(irq);
}
