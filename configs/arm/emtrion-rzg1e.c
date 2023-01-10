/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for emtrion's emCON-RZ/G1E board
 * (RZ/G1E dual-core Cortex-A7, 1G RAM)
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
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[22];
	struct jailhouse_irqchip irqchips[3];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0x7c000000,
			.size = 0x4000000,
		},
		.debug_console = {
			.address = 0xe62d0000,
			.size = 0x1000,
			.clock_reg = 0xe615014c,
			.gate_nr = 13,
			/* .divider = 0x2e, */
			.type = JAILHOUSE_CON_TYPE_HSCIF,
			.flags =  JAILHOUSE_CON_ACCESS_MMIO |
				  JAILHOUSE_CON_REGDIST_4 |
				  JAILHOUSE_CON_INVERTED_GATE,
		},
		.platform_info = {
			/* .pci_mmconfig_base = 0x2000000,
			.pci_mmconfig_end_bus = 0,
			.pci_is_virtual = 1, */
			.arm = {
				.gic_version = 2,
				.gicd_base = 0xf1001000,
				.gicc_base = 0xf1002000,
				.gich_base = 0xf1004000,
				.gicv_base = 0xf1006000,
				.maintenance_irq = 25,
			},
		},
		.root_cell = {
			.name = "emCON-RZ/G1E",
			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			/* .num_pci_devices = ARRAY_SIZE(config.pci_devices),
			.vpci_irq_base = 108, */
		},
	},

	.cpus = {
		0x3,
	},

	.mem_regions = {
		/* IVSHMEM shared memory region */
		JAILHOUSE_SHMEM_NET_REGIONS(0x7bf00000, 0),
		/* CPG */ {
			.phys_start = 0xe6150000,
			.virt_start = 0xe6150000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* APMU */ {
			.phys_start = 0xe6151000,
			.virt_start = 0xe6151000,
			.size = 0xf000,
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
		/* CMT0 */ {
			.phys_start = 0xffca0000,
			.virt_start = 0xffca0000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* CMT1 */ {
			.phys_start = 0xe6130000,
			.virt_start = 0xe6130000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* TMU0 */ {
			.phys_start = 0xe61e0000,
			.virt_start = 0xe61e0000,
			.size = 0x400,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* HSCIF2 */ {
			.phys_start = 0xe62d0000,
			.virt_start = 0xe62d0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* EtherAVB */ {
			.phys_start = 0xe6800000,
			.virt_start = 0xe6800000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMCIF: eMMC */ {
			.phys_start = 0xee200000,
			.virt_start = 0xee200000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* USB0 */ {
			.phys_start = 0xee080000,
			.virt_start = 0xee080000,
			.size = 0x00020000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* VSPDU */ {
			.phys_start = 0xfe930000,
			.virt_start = 0xfe930000,
			.size = 0x00008000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* DU */ {
			.phys_start = 0xfeb00000,
			.virt_start = 0xfeb00000,
			.size = 0x00040000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* CAN0 */ {
			.phys_start = 0xe6e80000,
			.virt_start = 0xe6e80000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
			},
		/* CAN1 */ {
			.phys_start = 0xe6e88000,
			.virt_start = 0xe6e88000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C0 */ {
			.phys_start = 0xe6508000,
			.virt_start = 0xe6508000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C1 */ {
			.phys_start = 0xe6530000,
			.virt_start = 0xe6530000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C2 */ {
			.phys_start = 0xe6540000,
			.virt_start = 0xe6540000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM */ {
			.phys_start = 0x40000000,
			.virt_start = 0x40000000,
			.size = 0x3bf00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0xf1001000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
			},
		},
		/* GIC */ {
			.address = 0xf1001000,
			.pin_base = 160,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
			},
		},
		/* GIC */ {
			.address = 0xf1001000,
			.pin_base = 288,
			.pin_bitmap = {
				0xffffffff
			},
		},
	},

	.pci_devices = {
		/* 00:01.0 */ {
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 1 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 0,
			.shmem_dev_id = 0,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
