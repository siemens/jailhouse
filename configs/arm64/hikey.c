/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for LeMaker HiKey board, 2 GB
 *
 * Copyright (c) Siemens AG, 2016-2022
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_system header;
	struct jailhouse_cpu cpus[8];
	struct jailhouse_memory mem_regions[8];
	struct jailhouse_irqchip irqchips[1];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0x7c000000,
			.size =       0x04000000,
		},
		.debug_console = {
			.address = 0xf7113000,
			.size = 0x1000,
			.type = JAILHOUSE_CON_TYPE_PL011,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
		.platform_info = {
			.pci_mmconfig_base = 0xf6000000,
			.pci_mmconfig_end_bus = 0,
			.pci_is_virtual = 1,
			.arm = {
				.gic_version = 2,
				.gicd_base = 0xf6801000,
				.gicc_base = 0xf6802000,
				.gich_base = 0xf6804000,
				.gicv_base = 0xf6806000,
				.maintenance_irq = 25,
			},
		},
		.root_cell = {
			.name = "HiKey",

			.num_cpus = ARRAY_SIZE(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),

			.vpci_irq_base = 136-32,
		},
	},

	.cpus = {
		{
			.phys_id = 0x0000,
		},
		{
			.phys_id = 0x0001,
		},
		{
			.phys_id = 0x0002,
		},
		{
			.phys_id = 0x0003,
		},
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
		/* IVSHMEM shared memory region */
		JAILHOUSE_SHMEM_NET_REGIONS(0x7bf00000, 0),
		/* MMIO (permissive) */ {
			.phys_start = 0xf4100000,
			.virt_start = 0xf4100000,
			.size =	      0x00008000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO (permissive) */ {
			.phys_start = 0xf7000000,
			.virt_start = 0xf7000000,
			.size =	      0x01100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM + mailbox? (permissive) */ {
			.phys_start = 0x0,
			.virt_start = 0x0,
			.size = 0x7bf00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* SRAM */ {
			.phys_start = 0xfff80000,
			.virt_start = 0xfff80000,
			.size = 0x12000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0xf6801000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
	},

	.pci_devices = {
		/* 0001:00:01.0 */ {
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 1,
			.bdf = 1 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 0,
			.shmem_dev_id = 0,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
