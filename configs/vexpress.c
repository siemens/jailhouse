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

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[8];
	struct jailhouse_irqchip irqchips[1];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.hypervisor_memory = {
			.phys_start = 0xfc000000,
			.size = 0x4000000,
		},
		.debug_console = {
			.address = 0x1c090000,
			.size = 0x1000,
			.flags = JAILHOUSE_CON1_TYPE_PL011 |
				 JAILHOUSE_CON1_ACCESS_MMIO |
				 JAILHOUSE_CON1_REGDIST_4 |
				 JAILHOUSE_CON2_TYPE_ROOTPAGE,
		},
		.platform_info.arm = {
#ifdef CONFIG_ARM_GIC_V3
			.gicd_base = 0x2f000000,
			.gicr_base = 0x2f100000,
#else /* GICv2 */
			.gicd_base = 0x2c001000,
			.gicc_base = 0x2c002000,
			.gich_base = 0x2c004000,
			.gicv_base = 0x2c006000,
#endif
			.maintenance_irq = 25,
		},
		.root_cell = {
			.name = "VExpress Linux",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = 1,
		},
	},

	.cpus = {
		0xf,
	},

	.mem_regions = {
		/* SMC91x */ {
			.phys_start = 0x1a000000,
			.virt_start = 0x1a000000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SP810 */ {
			.phys_start = 0x1c020000,
			.virt_start = 0x1c020000,
			.size = 0x00010000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMC */ {
			.phys_start = 0x1c050000,
			.virt_start = 0x1c050000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Mouse */ {
			.phys_start = 0x1c070000,
			.virt_start = 0x1c070000,
			.size = 0x00010000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Keyboard */ {
			.phys_start = 0x1c060000,
			.virt_start = 0x1c060000,
			.size = 0x00010000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* UARTs */ {
			.phys_start = 0x1c090000,
			.virt_start = 0x1c090000,
			.size = 0x00040000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Redistributors (ignore the mmio traps)*/ {
			.phys_start = 0x2f100000,
			.virt_start = 0x2f100000,
			.size = 0x04000000,
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
		/* GIC v3 */ {
			.address = 0x2f000000, /* GIC v2: 0x2f001000 */
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
			},
		},
	},

};
