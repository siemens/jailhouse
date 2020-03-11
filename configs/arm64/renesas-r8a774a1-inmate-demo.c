/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for inmate-demo on HopeRun HiHope RZ/G2M
 * platform based on r8a774a1/r8a774a3: 4xA53 CPUs, SCIF1.
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
	struct jailhouse_cpu cpus[1];
	struct jailhouse_memory mem_regions[8];
	struct jailhouse_irqchip irqchips[2];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.name = "renesas-r8a774a1-inmate-demo",
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
			.phys_id = 0x0001,
		},
	},

	.mem_regions = {
		/* IVSHMEM shared memory regions (demo) */
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
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_ROOTSHARED,
		},
		{
			.phys_start = 0xa900e000,
			.virt_start = 0xa900e000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* SCIF1 */ {
			.phys_start = 0xe6e68000,
			.virt_start = 0xe6e68000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* RAM */ {
			.phys_start = 0x89000000,
			.virt_start = 0x0,
			.size = 0x6400000,
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
		/* IVSHMEM */ {
			.address = 0xf1010000,
			.pin_base = 32,
			.pin_bitmap = {
				0x1000000, 0x0,
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
		{ /* IVSHMEM (demo) */
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 1,
			.bdf = 0 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 0,
			.shmem_dev_id = 1,
			.shmem_peers = 3,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_UNDEFINED,
		},
	},
};
