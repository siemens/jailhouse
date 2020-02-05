/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for Linux inmate on AM654 based platforms
 * 2 CPUs, 512MB RAM, 1 serial port(MCU UART)
 *
 * Copyright (c) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Authors:
 *  Lokesh Vutla <lokeshvutla@ti.com>
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
	struct jailhouse_memory mem_regions[12];
	struct jailhouse_irqchip irqchips[3];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.name = "k3-am654-idk-linux-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = ARRAY_SIZE(config.irqchips),
		.num_pci_devices = ARRAY_SIZE(config.pci_devices),
		.cpu_reset_address = 0x0,
		.vpci_irq_base = 189 - 32,

		.console = {
			.address = 0x40a00000,
			.divider = 0x35,
			.type = JAILHOUSE_CON_TYPE_8250,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
	},

	.cpus = {
		0xc,
	},

	.mem_regions = {
		/* IVSHMEM shared memory region for 00:01.0 */
		JAILHOUSE_SHMEM_NET_REGIONS(0x8dfb00000, 1),
		/* RAM load */ {
			.phys_start = 0x8FFFF0000,
			.virt_start = 0x0,
			.size = 0x10000,	/* 64KB */
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA |
				JAILHOUSE_MEM_LOADABLE,
		},
		/* RAM load */ {
			.phys_start = 0x8e0000000,
			.virt_start = 0x8e0000000,
			.size = 0x1fff0000,	/* (512MB - 64KB) */
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA |
				JAILHOUSE_MEM_LOADABLE,
		},
		/* MCU UART0 */ {
			.phys_start = 0x40a00000,
			.virt_start = 0x40a00000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* main sproxy target_data host_id=A53_3 */ {
			.phys_start = 0x3240f000,
			.virt_start = 0x3240f000,
			.size = 0x05000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* main sproxy rt host_id=A53_3 */ {
			.phys_start = 0x3280f000,
			.virt_start = 0x3280f000,
			.size = 0x05000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* main sproxy scfg host_id=A53_3 */ {
			.phys_start = 0x32c0f000,
			.virt_start = 0x32c0f000,
			.size = 0x05000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* communication region */ {
			.virt_start = 0x80000000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_COMM_REGION,
		},
	},

	.irqchips = {
		{
			.address = 0x01800000,
			.pin_base = 32,
			.pin_bitmap = {
			0x0, 0x80, 0x00, 0,
			},
		},
		{
			.address = 0x01800000,
			.pin_base = 160,
			.pin_bitmap = {
			1 << (190 - 160), 0x0, 0x00, 0,
			},
		},
		{
			.address = 0x01800000,
			.pin_base = 544,
			.pin_bitmap = {
			0, 0x200000, 0, 0,
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
