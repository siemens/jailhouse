/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for Jetson TK1 board
 * (NVIDIA Tegra K1 quad-core Cortex-A15, 2G RAM)
 *
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * NOTE: Add "mem=1984M vmalloc=512M" to the kernel command line.
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
			.phys_start = 0xfc000000,
			.size = 0x4000000 - 0x100000, /* -1MB (PSCI) */
		},
		.debug_uart = {
			.phys_start = 0x70006000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_IO,
		},
		.root_cell = {
			.name = "Jetson-TK1",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = 1,
		},
	},

	.cpus = {
		0xf,
	},

	.mem_regions = {
		/* PCIe */ {
			.phys_start = 0x01000000,
			.virt_start = 0x01000000,
			.size = 0x3f000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HACK: Legacy Interrupt Controller */ {
			.phys_start = 0x60004000,
			.virt_start = 0x60004000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HACK: Clock and Reset Controller */ {
			.phys_start = 0x60006000,
			.virt_start = 0x60006000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C5/6, SPI */ {
			.phys_start = 0x7000d000,
			.virt_start = 0x7000d000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HACK: Memory Controller */ {
			.phys_start = 0x70019000,
			.virt_start = 0x70019000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMC0/1 */ {
			.phys_start = 0x700b0000,
			.virt_start = 0x700b0000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RTC + PMC */ {
			.phys_start = 0x7000e000,
			.virt_start = 0x7000e000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* USB */ {
			.phys_start = 0x7d004000,
			.virt_start = 0x7d004000,
			.size = 0x00008000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* UART */ {
			.phys_start = 0x70006000,
			.virt_start = 0x70006000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM */ {
			.phys_start = 0x80000000,
			.virt_start = 0x80000000,
			.size = 0x7c000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
	},
	.irqchips = {
		/* GIC */ {
			.address = 0x50041000,
			.pin_bitmap = 0xffffffffffffffff,
		},
	},
};
