/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for Banana Pi board (A20 dual-core Cortex-A7, 1G RAM)
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
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[10];
	struct jailhouse_irqchip irqchips[1];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.hypervisor_memory = {
			.phys_start = 0x7c000000,
			.size = 0x4000000,
		},
		.debug_uart = {
			.phys_start = 0x01c28000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_IO,
		},
		.root_cell = {
			.name = "Banana-Pi",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = 1,
		},
	},

	.cpus = {
		0x3,
	},

	.mem_regions = {
		/* SPI */ {
			.phys_start = 0x01c05000,
			.virt_start = 0x01c05000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMC */ {
			.phys_start = 0x01c0f000,
			.virt_start = 0x01c0f000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* USB + PMU1 */ {
			.phys_start = 0x01c14000,
			.virt_start = 0x01c14000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SATA */ {
			.phys_start = 0x01c18000,
			.virt_start = 0x01c18000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* USB + PMU2 */ {
			.phys_start = 0x01c1c000,
			.virt_start = 0x01c1c000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* CCU, Ints, GPIO, Timer */ {
			.phys_start = 0x01c20000,
			.virt_start = 0x01c20000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* UART0-3 */ {
			.phys_start = 0x01c28000,
			.virt_start = 0x01c28000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* GMAC */ {
			.phys_start = 0x01c50000,
			.virt_start = 0x01c50000,
			.size = 0x00010000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HSTIMER */ {
			.phys_start = 0x01c60000,
			.virt_start = 0x01c60000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM */ {
			.phys_start = 0x40000000,
			.virt_start = 0x40000000,
			.size = 0x3c000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
	},
	.irqchips = {
		/* GIC */ {
			.address = 0x2f000000,
			.pin_bitmap = 0xffffffffffffffff,
		},
	},

};
