/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[16];
	struct jailhouse_irqchip irqchips[3];
	struct jailhouse_pci_device pci_devices[3];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.hypervisor_memory = {
			.phys_start = 0x82fc000000,
			.size =          0x4000000,
		},
		.debug_console = {
			.address = 0xe1010000,
			.size = 0x1000,
			.flags = JAILHOUSE_CON1_TYPE_PL011 |
				 JAILHOUSE_CON1_ACCESS_MMIO |
				 JAILHOUSE_CON1_REGDIST_4 |
				 JAILHOUSE_CON2_TYPE_ROOTPAGE,
		},
		.platform_info = {
			.pci_mmconfig_base = 0xf0000000,
			.pci_mmconfig_end_bus = 255,
			.arm = {
				.gicd_base = 0xe1110000,
				.gicc_base = 0xe112f000,
				.gich_base = 0xe1140000,
				.gicv_base = 0xe116f000,
				.maintenance_irq = 25,
			},
		},
		.root_cell = {
			.name = "amd-seattle",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),
		},
	},

	.cpus = {
		0xff,
	},

	.mem_regions = {
		/* gpio */ {
			.phys_start = 0xe0030000,
			.virt_start = 0xe0030000,
			.size =           0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* gpio */ {
			.phys_start = 0xe0080000,
			.virt_start = 0xe0080000,
			.size =           0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* gpio */ {
			.phys_start = 0xe1050000,
			.virt_start = 0xe1050000,
			.size =           0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* sata */ {
			.phys_start = 0xe0300000,
			.virt_start = 0xe0300000,
			.size =          0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* xgmac */ {
			.phys_start = 0xe0700000,
			.virt_start = 0xe0700000,
			.size =         0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* xgmac */ {
			.phys_start = 0xe0900000,
			.virt_start = 0xe0900000,
			.size =         0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* smmu */ {
			.phys_start = 0xe0600000,
			.virt_start = 0xe0600000,
			.size =          0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* smmu */ {
			.phys_start = 0xe0800000,
			.virt_start = 0xe0800000,
			.size =          0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* serial */ {
			.phys_start = 0xe1010000,
			.virt_start = 0xe1010000,
			.size =           0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ssp */ {
			.phys_start = 0xe1020000,
			.virt_start = 0xe1020000,
			.size =           0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ssp */ {
			.phys_start = 0xe1030000,
			.virt_start = 0xe1030000,
			.size =           0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* phy */ {
			.phys_start = 0xe1240000,
			.virt_start = 0xe1240000,
			.size =           0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* phy */ {
			.phys_start = 0xe1250000,
			.virt_start = 0xe1250000,
			.size =           0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ccn */ {
			.phys_start = 0xe8000000,
			.virt_start = 0xe8000000,
			.size =        0x1000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM */ {
			.phys_start = 0x8000000000,
			.virt_start = 0x8000000000,
			.size =        0x400000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* IVSHMEM shared memory region */ {
			.phys_start = 0x82fbe00000,
			.virt_start = 0x82fbe00000,
			.size =           0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
	},
	.irqchips = {
		/* GIC */ {
			.address = 0xe1110000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0xe1110000,
			.pin_base = 160,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0xe1110000,
			.pin_base = 288,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
	},
	.pci_devices = {
		/* 00:00.0 */ {
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.bdf = 0x0000,
		},
		/* 00:02.0 */ {
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.bdf = 0x0010,
		},
		/* 00:0f.0 */ {
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 0x0078,
			.bar_mask = {
				0xffffff00, 0xffffffff, 0x00000000,
				0x00000000, 0xffffffe0, 0xffffffff,
			},
			.num_msix_vectors = 1,
			.shmem_region = 15,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
