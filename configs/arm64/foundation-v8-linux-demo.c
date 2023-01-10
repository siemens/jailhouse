/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for Linux inmate on Foundation Model v8:
 * 2 CPUs, ~256MB RAM, serial port 3
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_cell_desc cell;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[3];
	struct jailhouse_irqchip irqchips[1];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.name = "linux-inmate-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = 1,
		.num_pci_devices = 0,

		.console = {
			.address = 0x1c090000,
			.type = JAILHOUSE_CON_TYPE_PL011,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
	},

	.cpus = {
		0xc, /* 2nd and 3rd CPUs */
	},

	/* Physical memory map:
	 * 0x0_0000_0000 - 0x0_7fff_ffff (2048 MiB) Devices
	 * 0x0_8000_0000 - 0x0_bbdf_ffff ( 958 MiB) Ram, root cell Kernel
	 * 0x0_bbe0_0000 - 0x0_fbff_ffff (1026 MiB) Ram, nothing
	 * 0x0_fc00_0000 - 0x1_0000_0000 (  64 MiB) Ram, hypervisor
	 * ...                           (  30 GiB)
	 * 0x8_8000_0000 - 0x9_0000_0000 (2048 MiB) Ram, nonroot cells
	 */
	.mem_regions = {
		/* uart3 */ {
			.phys_start = 0x1c0c0000,
			.virt_start = 0x1c090000,	/* inmate lib uses */
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM load */ {
			.phys_start = 0x880000000,
			.virt_start = 0x0,
			.size = 0x10000000,	/* 256 MiB */
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_LOADABLE,
		},
		/* communication region */ {
			.virt_start = 0x80000000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_COMM_REGION,
		},
	},

	.irqchips = {
		/* GIC v2 */ {
			.address = 0x2c001000, /* GIC v3: 0x2f000000 */
			.pin_base = 32,
			.pin_bitmap = {
				(1 << 8) /* uart3 */
			},
		},
	}
};
