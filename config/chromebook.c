/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for Samsung Chromebook, 2 GB RAM, 64 MB hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/types.h>
#include <jailhouse/cell-config.h>

#define ALIGN __attribute__((aligned(1)))
#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_system ALIGN header;
	__u64 ALIGN cpus[1];
	struct jailhouse_memory ALIGN mem_regions[1];
} ALIGN config = {
	.header = {
		.hypervisor_memory = {
			.phys_start = 0xbc000000,
			.size = 0x4000000,
		},
		.system = {
			.name = "Samsung Chromebook",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irq_lines = 0,
			.pio_bitmap_size = 0,

			.num_pci_devices = 0,
		},
	},

	.cpus = {
		0xf,
	},

	.mem_regions = {
		/* RAM */ {
			.phys_start = 0x0,
			.virt_start = 0x0,
			.size = 0x3c000000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE | JAILHOUSE_MEM_EXECUTE,
		},
	},
};
