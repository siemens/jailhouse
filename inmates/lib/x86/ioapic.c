/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

#define IOAPIC_BASE		((void *)0xfec00000)
#define IOAPIC_REG_INDEX	0x00
#define IOAPIC_REG_DATA		0x10
#define IOAPIC_REDIR_TBL_START	0x10

void ioapic_init(void)
{
	map_range(IOAPIC_BASE, PAGE_SIZE, MAP_UNCACHED);
}

void ioapic_pin_set_vector(unsigned int pin,
			   enum ioapic_trigger_mode trigger_mode,
			   unsigned int vector)
{
	mmio_write32(IOAPIC_BASE + IOAPIC_REG_INDEX,
		     IOAPIC_REDIR_TBL_START + pin * 2 + 1);
	mmio_write32(IOAPIC_BASE + IOAPIC_REG_DATA, cpu_id() << (56 - 32));

	mmio_write32(IOAPIC_BASE + IOAPIC_REG_INDEX,
		     IOAPIC_REDIR_TBL_START + pin * 2);
	mmio_write32(IOAPIC_BASE + IOAPIC_REG_DATA, trigger_mode | vector);
}
