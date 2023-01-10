/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for linux-demo inmate on emCON-RZ/G1E:
 * 1 CPU, 64M RAM, I2C bus I2C2, serial port SCIF4, SDHI0
 *
 * Copyright (c) emtrion GmbH, 2017
 *
 * Authors:
 *  Ruediger Fichter <ruediger.fichter@emtrion.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_cell_desc cell;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[14];
	struct jailhouse_irqchip irqchips[3];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM,
		.name = "emtrion-emconrzg1e-linux-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = ARRAY_SIZE(config.irqchips),
		/* .num_pci_devices = ARRAY_SIZE(config.pci_devices),
		.vpci_irq_base = 123, */

		.console = {
			.address = 0xe6ee0000,
			.clock_reg = 0xe615014c,
			.gate_nr = 15,
			.divider = 0x10,
			.type = JAILHOUSE_CON_TYPE_HSCIF,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
	},

	.cpus = {
		0x2,
	},

	.mem_regions = {
		/* IVSHMEM shared memory region */
		JAILHOUSE_SHMEM_NET_REGIONS(0x7bf00000, 1),
		/* RST, MODEMR */ {
			.phys_start = 0xe6160060,
			.virt_start = 0xe6160060,
			.size = 0x4,
			.flags = JAILHOUSE_MEM_READ |
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
		/* communication region */ {
			.virt_start = 0x80000000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_COMM_REGION,
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
		/* 00:01.0 */ {
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 1 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 0,
			.shmem_dev_id = 1,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
