/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for emtrion's emCON-RZ/G1H board
 * (RZ/G1H octa-core Cortex-A7/Cortex-A15, 2G RAM)
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

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[38];
	struct jailhouse_irqchip irqchips[3];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0xbc000000,
			.size = 0x4000000,
		},
		.debug_console = {
			.address = 0xe6c50000,
			.size = 0x1000,
			.clock_reg = 0xe6150138,
			.gate_nr = 3,
			/* .divider = 0x2e, */
			.type= JAILHOUSE_CON_TYPE_SCIFA,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
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
			.name = "emCON-RZ/G1H",
			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			/* .num_pci_devices = ARRAY_SIZE(config.pci_devices),
			.vpci_irq_base = 108, */
		},
	},

	.cpus = {
		0xff,
	},

	.mem_regions = {
		/* IVSHMEM shared memory region */
		JAILHOUSE_SHMEM_NET_REGIONS(0x7bf00000, 0),
		/* SYS-DMAC */ {
			.phys_start = 0xe6700000,
			.virt_start = 0xe6700000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SYS-DMAC */ {
			.phys_start = 0xe6720000,
			.virt_start = 0xe6720000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SDHI1 */ {
			.phys_start = 0xee120000,
			.virt_start = 0xee120000,
			.size = 0x20000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* PWM */ {
			.phys_start = 0xe6e30000,
			.virt_start = 0xe6e30000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* R-GP2D */ {
			.phys_start = 0xe6ec0000,
			.virt_start = 0xe6ec0000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Thermal Sensor */ {
			.phys_start = 0xe61f0000,
			.virt_start = 0xe61f0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* CPG */ {
			.phys_start = 0xe6150000,
			.virt_start = 0xe6150000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* APMU */ {
			.phys_start = 0xe6151000,
			.virt_start = 0xe6151000,
			.size = 0xf000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RES */ {
			.phys_start = 0xe6160000,
			.virt_start = 0xe6160000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* IRQC */ {
			.phys_start = 0xe61c0000,
			.virt_start = 0xe61c0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* CMT0 */ {
			.phys_start = 0xffca0000,
			.virt_start = 0xffca0000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* CMT1 */ {
			.phys_start = 0xe6130000,
			.virt_start = 0xe6130000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* TMU0 */ {
			.phys_start = 0xe61e0000,
			.virt_start = 0xe61e0000,
			.size = 0x400,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* SCIFA1 */ {
			.phys_start = 0xe6c50000,
			.virt_start = 0xe6c50000,
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
			.phys_start = 0xee220000,
			.virt_start = 0xee220000,
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
		/* DU */ {
			.phys_start = 0xfeb00000,
			.virt_start = 0xfeb00000,
			.size = 0x00040000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* LVDS */ {
			.phys_start = 0xfeb90000,
			.virt_start = 0xfeb90000,
			.size = 0x00010000,
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
			.phys_start = 0xe6518000,
			.virt_start = 0xe6518000,
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
		/* I2C3 */ {
			.phys_start = 0xe6540000,
			.virt_start = 0xe6540000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* IIC0 */ {
			.phys_start = 0xe6500000,
			.virt_start = 0xe6500000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* IIC1 */ {
			.phys_start = 0xe6510000,
			.virt_start = 0xe6510000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* IIC2 */ {
			.phys_start = 0xe6520000,
			.virt_start = 0xe6520000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* IIC3 */ {
			.phys_start = 0xe60b0000,
			.virt_start = 0xe60b0000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* VSPD0 */ {
			.phys_start = 0xfe920000,
			.virt_start = 0xfe920000,
			.size = 0x00008000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* VSPD1 */ {
			.phys_start = 0xfe928000,
			.virt_start = 0xfe928000,
			.size = 0x00008000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* VSPD2 */ {
			.phys_start = 0xfe930000,
			.virt_start = 0xfe930000,
			.size = 0x00008000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* VSPD3 */ {
			.phys_start = 0xfe938000,
			.virt_start = 0xfe938000,
			.size = 0x00008000,
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
