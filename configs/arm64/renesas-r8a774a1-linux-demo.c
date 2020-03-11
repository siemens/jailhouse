/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for linux-demo inmate on HopeRun HiHope RZ/G2M
 * platform based on r8a774a1/r8a774a3: 4xA53 CPUs, PFC, SCIF1,
 * GPIO6, PRR and LED1.
 *
 * Copyright (c) 2023, Renesas Electronics Corporation
 *
 * Authors:
 *  Lad Prabhakar <prabhakar.mahadev-lad.rj@bp.renesas.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_cell_desc cell;
	struct jailhouse_cpu cpus[4];
	struct jailhouse_memory mem_regions[16];
	struct jailhouse_irqchip irqchips[2];
	struct jailhouse_pci_device pci_devices[2];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.name = "renesas-r8a774a1-linux-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG |
			 JAILHOUSE_CELL_VIRTUAL_CONSOLE_ACTIVE,

		.num_cpus = ARRAY_SIZE(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = ARRAY_SIZE(config.irqchips),
		.num_pci_devices = ARRAY_SIZE(config.pci_devices),

		.vpci_irq_base = 24,

		.console = {
			.address = 0xe6e68000,
			.size = 0x40,
			.type = JAILHOUSE_CON_TYPE_SCIF,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
	},

	.cpus = {
		{
			.phys_id = 0x0100,
		},
		{
			.phys_id = 0x0101,
		},
		{
			.phys_id = 0x0102,
		},
		{
			.phys_id = 0x0103,
		},
	},

	.mem_regions = {
		/* IVSHMEM shared memory regions for 00:00.0 (demo) */
		{
			.phys_start = 0xa9000000,
			.virt_start = 0xa9000000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_ROOTSHARED,
		},
		{
			.phys_start = 0xa9001000,
			.virt_start = 0xa9001000,
			.size = 0x9000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_ROOTSHARED,
		},
		{
			.phys_start = 0xa900a000,
			.virt_start = 0xa900a000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_ROOTSHARED,
		},
		{
			.phys_start = 0xa900c000,
			.virt_start = 0xa900c000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_ROOTSHARED,
		},
		{
			.phys_start = 0xa900e000,
			.virt_start = 0xa900e000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_ROOTSHARED,
		},
		/* IVSHMEM shared memory regions for 00:01.0 (networking) */
		JAILHOUSE_SHMEM_NET_REGIONS(0xa9010000, 1),
		/* GPIO6 */ {
			.phys_start = 0xe6055400,
			.virt_start = 0xe6055400,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED |
				 JAILHOUSE_MEM_ROOTSHARED,
		},
		/* PFC */ {
			.phys_start = 0xe6060000,
			.virt_start = 0xe6060000,
			.size = 0x50c,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED |
				 JAILHOUSE_MEM_ROOTSHARED,
		},
		/* SCIF1 */ {
			.phys_start = 0xe6e68000,
			.virt_start = 0xe6e68000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PRR */ {
			.phys_start = 0xfff00044,
			.virt_start = 0xfff00044,
			.size = 0x4,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED |
				 JAILHOUSE_MEM_ROOTSHARED,
		},
		/* linux-loader space */ {
			.phys_start = 0x89000000,
			.virt_start = 0x0,
			.size = 0x6400000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_LOADABLE,
		},
		/* RAM */ {
			.phys_start = 0x8f400000,
			.virt_start = 0x8f400000,
			.size = 0x19c00000,
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
		/* IVSHMEM */ {
			.address = 0xf1010000,
			.pin_base = 32,
			.pin_bitmap = {
				0xf000000, 0x0,
			},
		},
		/* SCIF1 */ {
			.address = 0xf1010000,
			.pin_base = 160,
			.pin_bitmap = {
				0x2000000, 0x0,
			},
		},
	},

	.pci_devices = {
		{ /* IVSHMEM 00:00.0 (demo) */
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 1,
			.bdf = 0 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 0,
			.shmem_dev_id = 2,
			.shmem_peers = 3,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_UNDEFINED,
		},
		{ /* IVSHMEM 00:01.0 (networking) */
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 1 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 5,
			.shmem_dev_id = 1,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
