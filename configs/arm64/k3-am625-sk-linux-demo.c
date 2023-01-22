/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for Linux inmate on AM625 based platforms
 * 3 CPUs, 512MB RAM, 1 serial port (MAIN UART1)
 *
 * Copyright (c) 2022 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Authors:
 *  Matt Ranostay <mranostay@ti.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#ifndef CONFIG_INMATE_BASE
#define CONFIG_INMATE_BASE 0x0000000
#endif

struct {
	struct jailhouse_cell_desc cell;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[18];
	struct jailhouse_irqchip irqchips[3];
	struct jailhouse_pci_device pci_devices[2];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.name = "k3-am625-sk-linux-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = ARRAY_SIZE(config.irqchips),
		.num_pci_devices = ARRAY_SIZE(config.pci_devices),
		.cpu_reset_address = 0x0,
		.vpci_irq_base = 189 - 32,

		.console = {
			.address = 0x02810000,
			.divider = 0x1b,
			.type = JAILHOUSE_CON_TYPE_8250,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
	},

	.cpus = {
		0xc,
	},

	.mem_regions = {
		/* IVSHMEM shared memory regions for 00:00.0 (demo) */
		{
			.phys_start = 0xdfa00000,
			.virt_start = 0xdfa00000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_ROOTSHARED,
		},
		{
			.phys_start = 0xdfa10000,
			.virt_start = 0xdfa10000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_ROOTSHARED,
		},
		/* Peer 0 */ {
			.phys_start = 0xdfa20000,
			.virt_start = 0xdfa20000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* Peer 1 */ {
			.phys_start = 0xdfa30000,
			.virt_start = 0xdfa30000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* Peer 2 */ {
			.phys_start = 0xdfa40000,
			.virt_start = 0xdfa40000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_ROOTSHARED,
		},
		/* IVSHMEM shared memory region for 00:01.0 */
		JAILHOUSE_SHMEM_NET_REGIONS(0xdfb00000, 1),
		/* RAM load */ {
			.phys_start = 0xffff0000,
			.virt_start = 0x0,
			.size = 0x10000,	/* 64KB */
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA |
				JAILHOUSE_MEM_LOADABLE,
		},
		/* RAM load */ {
			.phys_start = 0xe0000000,
			.virt_start = 0xe0000000,
			.size = 0x1fff0000,	/* (512MB - 64KB) */
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA |
				JAILHOUSE_MEM_LOADABLE,
		},
		/* MAIN UART1 */ {
			.phys_start = 0x02810000,
			.virt_start = 0x02810000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
#ifdef CONFIG_ENABLE_AM625_INMATE_CELL_EMMC
		/* sdhci0 */ {
			.phys_start = 0x0fa10000,
			.virt_start = 0x0fa10000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* sdhci0 */ {
			.phys_start = 0x0fa18000,
			.virt_start = 0x0fa18000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
#endif
		/* main sproxy target_data host_id=A53_3 */ {
			.phys_start = 0x4d00e000,
			.virt_start = 0x4d00e000,
			.size = 0x3000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* main sproxy rt host_id=A53_3 */ {
			.phys_start = 0x4a60e000,
			.virt_start = 0x4a60e000,
			.size = 0x3000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* main sproxy scfg host_id=A53_3 */ {
			.phys_start = 0x4a40e000,
			.virt_start = 0x4a40e000,
			.size = 0x3000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* communication region */ {
			.virt_start = 0x80000000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_COMM_REGION,
		},
	},

	.irqchips = {
		/*
		 * offset = (SPI_NR + 32 - base) / 32
		 * bit = (SPI_NR + 32 - base) % 32
		 */
		{
			.address = 0x01800000,
			.pin_base = 32,
			.pin_bitmap = {
			0,
			/* sproxy rx_014 */
			1 << (67 - 64),
			0, 0
			},
		},
		{
			.address = 0x01800000,
			.pin_base = 160,
			.pin_bitmap = {
#ifdef CONFIG_ENABLE_AM625_INMATE_CELL_EMMC
			/* sdhc */
			1 << (165 - 160) |
#endif
			/* vpci */
			1 << (189 - 160) |
			1 << (190 - 160),
			/* uart */
			1 << (211 - 192),
			0, 0,
			},
		},
		{
			.address = 0x01800000,
			.pin_base = 544,
			.pin_bitmap = {
			0, 0, 0, 0,
			},
		},
	},

	.pci_devices = {
		/* 00:00.0 */ {
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 0 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX_64K,
			.shmem_regions_start = 0,
			.shmem_dev_id = 2,
			.shmem_peers = 3,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_UNDEFINED,
		},
		/* 00:01.0 */ {
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 1 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX_64K,
			.shmem_regions_start = 5,
			.shmem_dev_id = 1,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
