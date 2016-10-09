/*
 * LeMaker HiKey board, 2 GB
 *
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[2];
	struct jailhouse_irqchip irqchips[1];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.hypervisor_memory = {
			.phys_start = 0x7c000000,
			.size =       0x04000000,
		},
		.debug_console = {
			.phys_start = 0xf7113000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_IO,
		},
		.platform_info.arm = {
			.gicd_base = 0xf6801000,
			.gicc_base = 0xf6802000,
			.gich_base = 0xf6804000,
			.gicv_base = 0xf6806000,
			.maintenance_irq = 25,
		},
		.root_cell = {
			.name = "HiKey",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
		},
	},

	.cpus = {
		0xff,
	},

	.mem_regions = {
		/* MMIO (permissive) */ {
			.phys_start = 0xf7000000,
			.virt_start = 0xf7000000,
			.size =	      0x01100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM + mailbox? (permissive) */ {
			.phys_start = 0x0,
			.virt_start = 0x0,
			.size = 0x7c000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
	},
	.irqchips = {
		/* GIC */ {
			.address = 0xf6801000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
	},

};
