/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for linux-demo inmate on emCON-RZ/G1M:
 * 1 CPU, 64M RAM, I2C bus I2C2, serial port SCIF4, SDHI0
 *
 * Copyright (c) emtrion GmbH, 2017
 *
 * Authors:
 *  Jan von Wiarda <jan.vonwiarda@emtrion.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_cell_desc cell;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[13];
	struct jailhouse_irqchip irqchips[3];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.name = "emtrion-emconrzg1m-linux-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = ARRAY_SIZE(config.irqchips),
		/* .num_pci_devices = ARRAY_SIZE(config.pci_devices),
		.vpci_irq_base = 123, */
	},

	.cpus = {
		0x2,
	},

	.mem_regions = {
		/* CPG (HACK) */ {
			.phys_start = 0xe6150000,
			.virt_start = 0xe6150000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* RST, MODEMR */ {
			.phys_start = 0xe6160060,
			.virt_start = 0xe6160060,
			.size = 0x4,
			.flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* RST, CA15RESCNT */ {
			.phys_start = 0xe6160040,
			.virt_start = 0xe6160040,
			.size = 0x4,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* IRQC */ {
			.phys_start = 0xe61c0000,
			.virt_start = 0xe61c0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* Generic Counter */ {
			.phys_start = 0xe6080000,
			.virt_start = 0xe6080000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* PFC (HACK) */ {
			.phys_start = 0xe6060000,
			.virt_start = 0xe6060000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* SYSC (HACK) */ {
			.phys_start = 0xe6180000,
			.virt_start = 0xe6180000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* SCIF4 */ {
			.phys_start = 0xe6ee0000,
			.virt_start = 0xe6ee0000,
			.size = 0x400,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32 |
				JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_8,
		},
		/* SDHI0: SDC */ {
			.phys_start = 0xee100000,
			.virt_start = 0xee100000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C2 */ {
			.phys_start = 0xe6530000,
			.virt_start = 0xe6530000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM */ {
			.phys_start = 0x7bef0000,
			.virt_start = 0,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_LOADABLE,
		},
		/* RAM */ {
			.phys_start = 0x70000000,
			.virt_start = 0x70000000,
			.size = 0xbef0000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA |
				JAILHOUSE_MEM_LOADABLE,
		},
		/* IVSHMEM shared memory region */ {
			.phys_start = 0x7bf00000,
			.virt_start = 0x7bf00000,
			.size = 0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_ROOTSHARED,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0xf1001000,
			.pin_base = 32,
			.pin_bitmap = {
				1 << (24+32 - 32), /* SCIF4 */
			},
		},
		/* GIC */ {
			.address = 0xf1001000,
			.pin_base = 160,
			.pin_bitmap = {
				0, 1 << (165+32 - 192) /* SDHI0 */
			},
		},
		/* GIC */ {
			.address = 0xf1001000,
			.pin_base = 288,
			.pin_bitmap = {
				1 << (286+32 - 288), /* I2C2 */
			},
		},
	},

	.pci_devices = {
		/* 00:00.0 */ {
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 0x00,
			.bar_mask = {
				0xffffff00, 0xffffffff, 0x00000000,
				0x00000000, 0x00000000, 0x00000000,
			},
			.shmem_region = 4,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
