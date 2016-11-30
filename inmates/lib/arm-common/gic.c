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

#include <inmate.h>
#include <gic.h>

static irq_handler_t irq_handler = (irq_handler_t)NULL;

/* Replaces the weak reference in header.S */
void vector_irq(void)
{
	u32 irqn;

	while (1) {
		irqn = gic_read_ack();
		if (irqn == 0x3ff)
			break;

		if (irq_handler)
			irq_handler(irqn);

		gic_write_eoi(irqn);
	}
}

void gic_setup(irq_handler_t handler)
{
	gic_init();
	irq_handler = handler;

	gic_setup_irq_stack();
}

void gic_enable_irq(unsigned int irq)
{
	gic_enable(irq);
}
