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

#include <asm/irqchip.h>
#include <asm/sysregs.h>
#include <jailhouse/entry.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>

/*
 * The init function must be called after the MMU setup, and whilst in the
 * per-cpu setup, which means that a bool must be set by the master CPU
 */
static bool irqchip_is_init;
static struct irqchip_ops irqchip;

void irqchip_handle_irq(struct per_cpu *cpu_data)
{
	irqchip.handle_irq(cpu_data);
}

int irqchip_send_sgi(struct sgi *sgi)
{
	return irqchip.send_sgi(sgi);
}

int irqchip_cpu_init(struct per_cpu *cpu_data)
{
	if (irqchip.cpu_init)
		return irqchip.cpu_init(cpu_data);

	return 0;
}

int irqchip_init(void)
{
	/* Only executed on master CPU */
	if (irqchip_is_init)
		return 0;

	memset(&irqchip, 0, sizeof(irqchip));
	irqchip_is_init = true;

	return -ENODEV;
}
