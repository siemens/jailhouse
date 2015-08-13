/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Minimal configuration for IOAPIC demo inmate:
 * 1 CPU, 1 MB RAM, 1 serial port, 1 ACPI IRQ pin
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_cell_desc cell;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[2];
	struct jailhouse_irqchip irqchips[1];
	__u8 pio_bitmap[0x2000];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.name = "ioapic-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = ARRAY_SIZE(config.irqchips),
		.pio_bitmap_size = ARRAY_SIZE(config.pio_bitmap),
		.num_pci_devices = 0,
	},

	.cpus = {
		0x4,
	},

	.mem_regions = {
		/* RAM */ {
			.phys_start = 0x3f100000,
			.virt_start = 0,
			.size = 0x00100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_LOADABLE,
		},
		/* communication region */ {
			.virt_start = 0x00100000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_COMM_REGION,
		},
	},

	.irqchips = {
		/* IOAPIC */ {
			.address = 0xfec00000,
			.id = 0xff01,
			.pin_bitmap = 0x000200, /* ACPI IRQ */
		},
	},

	.pio_bitmap = {
		[     0/8 ...  0x2f7/8] = -1,
		[ 0x2f8/8 ...  0x2ff/8] = 0, /* serial2 */
		[ 0x300/8 ...  0x5ff/8] = -1,
		[ 0x600/8 ...  0x607/8] = 0xf0, /* acpi-evt */
		[ 0x608/8 ...  0x7ff/8] = -1,
		[ 0x800/8 ...  0x807/8] = 0xf0 /* apci-pm1a */,
		[ 0x808/8 ... 0xdfff/8] = -1,
		[0xe000/8 ... 0xe007/8] = 0, /* OXPCIe952 serial2 */
		[0xe008/8 ... 0xffff/8] = -1,
	},
};
