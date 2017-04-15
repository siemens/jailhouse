/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for Orange Pi Zero (H2+ quad-core Cortex-A7, 256MB RAM)
 *
 * Copyright (c) Siemens AG, 2014-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
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
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.hypervisor_memory = {
			.phys_start = 0x4f800000,
			.size = 0x800000,
		},
		.debug_console = {
			.address = 0x01c28000,
			.size = 0x1000,
			.flags = JAILHOUSE_CON1_TYPE_8250 |
				 JAILHOUSE_CON1_ACCESS_MMIO |
				 JAILHOUSE_CON1_REGDIST_4 |
				 JAILHOUSE_CON2_TYPE_ROOTPAGE,
		},
		.platform_info = {
			.pci_mmconfig_base = 0x2000000,
			.pci_mmconfig_end_bus = 0,
			.pci_is_virtual = 1,
			.arm = {
				.gicd_base = 0x01c81000,
				.gicc_base = 0x01c82000,
				.gich_base = 0x01c84000,
				.gicv_base = 0x01c86000,
				.maintenance_irq = 25,
			},
		},
		.root_cell = {
			.name = "Orange-Pi0",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),

			.vpci_irq_base = 108,
		},
	},

	.cpus = {
		0xf,
	},

	.mem_regions = {
		/* MMIO 1 (permissive) */ {
			.phys_start = 0x01c00000,
			.virt_start = 0x01c00000,
			.size =          0x20000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* pinctrl PA */ {
			.phys_start = 0x01c20800,
			.virt_start = 0x01c20800,
			.size =             0x24,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* pinctrl rest */ {
			.phys_start = 0x01c20824,
			.virt_start = 0x01c20824,
			.size =            0x3dc,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* watchdog */ {
			.phys_start = 0x01c20ca0,
			.virt_start = 0x01c20ca0,
			.size =             0x20,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* MMIO 2 (permissive) */ {
			.phys_start = 0x01c28000,
			.virt_start = 0x01c28000,
			.size =           0x9000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO 3 (permissive) */ {
			.phys_start = 0x01f01000,
			.virt_start = 0x01f01000,
			.size =           0x3000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM */ {
			.phys_start = 0x40000000,
			.virt_start = 0x40000000,
			.size =        0xf700000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* IVSHMEM shared memory region */ {
			.phys_start = 0x4f700000,
			.virt_start = 0x4f700000,
			.size =         0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0x01c81000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
			},
		},
	},

	.pci_devices = {
		{
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 0x00,
			.bar_mask = {
				0xffffff00, 0xffffffff, 0x00000000,
				0x00000000, 0x00000000, 0x00000000,
			},
			.shmem_region = 7,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
