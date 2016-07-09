/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for linux inmate, 1 CPU, ~60 MB RAM, 1 serial port
 *
 * Copyright (c) Siemens AG, 2013-2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_cell_desc cell;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[4];
	struct jailhouse_cache cache_regions[1];
	__u8 pio_bitmap[0x2000];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.name = "linux-x86-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_cache_regions = ARRAY_SIZE(config.cache_regions),
		.num_irqchips = 0,
		.pio_bitmap_size = ARRAY_SIZE(config.pio_bitmap),
		.num_pci_devices = ARRAY_SIZE(config.pci_devices),
		.num_pci_caps = 0,
	},

	.cpus = {
		0xe,
	},

	.mem_regions = {
		/* low RAM */ {
			.phys_start = 0x3b600000,
			.virt_start = 0,
			.size = 0x00100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA |
				JAILHOUSE_MEM_LOADABLE,
		},
		/* communication region */ {
			.virt_start = 0x00100000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_COMM_REGION,
		},
		/* high RAM */ {
			.phys_start = 0x3b700000,
			.virt_start = 0x00200000,
			.size = 0x3a00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA |
				JAILHOUSE_MEM_LOADABLE,
		},
		/* IVSHMEM shared memory region */
		{
			.phys_start = 0x3f100000,
			.virt_start = 0x3f100000,
			.size = 0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_ROOTSHARED,
		},
	},

	.cache_regions = {
		{
			.start = 0,
			.size = 2,
			.type = JAILHOUSE_CACHE_L3,
		},
	},

	.pio_bitmap = {
		[     0/8 ...  0x3f7/8] = -1,
		[ 0x3f8/8 ...  0x3ff/8] = 0, /* serial1 */
		[ 0x400/8 ... 0xe00f/8] = -1,
		[0xe010/8 ... 0xe017/8] = 0, /* OXPCIe952 serial1 */
		[0xe018/8 ... 0xffff/8] = -1,
	},

	.pci_devices = {
		{
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 0x0,
			.bdf = (0x0f<<3),
			.bar_mask = {
				0xffffff00, 0xffffffff, 0x00000000,
				0x00000000, 0xffffffe0, 0xffffffff,
			},
			.shmem_region = 3,
			.num_msix_vectors = 1,
		},
	},
};
