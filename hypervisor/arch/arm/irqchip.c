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

#include <jailhouse/entry.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <asm/gic_common.h>
#include <asm/irqchip.h>
#include <asm/platform.h>
#include <asm/setup.h>
#include <asm/sysregs.h>

/* AMBA's biosfood */
#define AMBA_DEVICE	0xb105f00d

void *gicd_base;
unsigned long gicd_size;

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

/* Only the GIC is implemented */
extern struct irqchip_ops gic_irqchip;

int irqchip_init(void)
{
	int i, err;
	u32 pidr2, cidr;
	u32 dev_id = 0;

	/* Only executed on master CPU */
	if (irqchip_is_init)
		return 0;

	/* FIXME: parse device tree */
	gicd_base = GICD_BASE;
	gicd_size = GICD_SIZE;

	if ((err = arch_map_device(gicd_base, gicd_base, gicd_size)) != 0)
		return err;

	for (i = 3; i >= 0; i--) {
		cidr = mmio_read32(gicd_base + GICD_CIDR0 + i * 4);
		dev_id |= cidr << i * 8;
	}
	if (dev_id != AMBA_DEVICE)
		goto err_no_distributor;

	/* Probe the GIC version */
	pidr2 = mmio_read32(gicd_base + GICD_PIDR2);
	switch (GICD_PIDR2_ARCH(pidr2)) {
	case 0x2:
		break;
	case 0x3:
	case 0x4:
		memcpy(&irqchip, &gic_irqchip, sizeof(struct irqchip_ops));
		break;
	}

	if (irqchip.init) {
		err = irqchip.init();
		irqchip_is_init = true;

		return err;
	}

err_no_distributor:
	printk("GIC: no distributor found\n");
	arch_unmap_device(gicd_base, gicd_size);

	return -ENODEV;
}
