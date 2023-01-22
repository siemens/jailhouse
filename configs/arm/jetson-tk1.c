/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for Jetson TK1 board
 * (NVIDIA Tegra K1 quad-core Cortex-A15, 2G RAM)
 *
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * NOTE: Add "mem=1920M vmalloc=512M" to the kernel command line.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[25];
	struct jailhouse_irqchip irqchips[2];
	struct jailhouse_pci_device pci_devices[1];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0xfc000000,
			.size = 0x4000000 - 0x100000, /* -1MB (PSCI) */
		},
		.debug_console = {
			.address = 0x70006300,
			.size = 0x40,
			/* .clock_reg = 0x60006000 + 0x330, */
			/* .gate_nr = (65 % 32), */
			/* .divider = 0xdd, */
			.type = JAILHOUSE_CON_TYPE_8250,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
		.platform_info = {
			.pci_mmconfig_base = 0x48000000,
			.pci_mmconfig_end_bus = 0,
			.pci_is_virtual = 1,
			.pci_domain = -1,
			.arm = {
				.gic_version = 2,
				.gicd_base = 0x50041000,
				.gicc_base = 0x50042000,
				.gich_base = 0x50044000,
				.gicv_base = 0x50046000,
				.maintenance_irq = 25,
			},
		},
		.root_cell = {
			.name = "Jetson-TK1",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),

			.vpci_irq_base = 148,
		},
	},

	.cpus = {
		0xf,
	},

	.mem_regions = {
		/* IVSHMEM shared memory region */
		JAILHOUSE_SHMEM_NET_REGIONS(0xfbf00000, 0),
		/* PCIe */ {
			.phys_start = 0x01000000,
			.virt_start = 0x01000000,
			.size = 0x3f000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/*50000000-50033fff : /host1x@0,50000000*/ {
			.phys_start = 0x50000000,
			.virt_start = 0x50000000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/*54200000-5423ffff : /host1x@0,50000000/dc@0,54200000*/ {
			.phys_start = 0x54200000,
			.virt_start = 0x54200000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/*54240000-5427ffff : /host1x@0,50000000/dc@0,54240000*/ {
			.phys_start = 0x54240000,
			.virt_start = 0x54240000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/*54280000-542bffff : /host1x@0,50000000/hdmi@0,54280000 */ {
			.phys_start = 0x54280000,
			.virt_start = 0x54280000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HACK: GPU */ {
			.phys_start = 0x57000000,
			.virt_start = 0x57000000,
			.size = 0x02000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/*
		 * HACK: 0x1400@0x60002000: APBDMA
		 * HACK: 0x0500@0x60004000: Legacy Interrupt Controller,
		 * HACK: 0x0400@0x60005000: Nvidia Timers (TMR + WDT),
		 * HACK: 0x1000@0x60006000: Clock and Reset Controller
		 */ {
			.phys_start = 0x60002000,
			.virt_start = 0x60002000,
			.size = 0x5000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* GPIO */ {
			.phys_start = 0x6000d000,
			.virt_start = 0x6000d000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HACK: apbmisc "Chip Revision" */ {
			.phys_start = 0x70000800,
			.virt_start = 0x70000800,
			.size = 0x00000100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* UART */ {
			.phys_start = 0x70006000,
			.virt_start = 0x70006000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C, including HDMI_DDC*/ {
			.phys_start = 0x7000c000,
			.virt_start = 0x7000c000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C5/6, SPI */ {
			.phys_start = 0x7000d000,
			.virt_start = 0x7000d000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RTC + PMC + apbmisc "Strapping Options" */ {
			.phys_start = 0x7000e000,
			.virt_start = 0x7000e000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HACK: Memory Controller */ {
			.phys_start = 0x70019000,
			.virt_start = 0x70019000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* XUSB_HOST */ {
			.phys_start = 0x70090000,
			.virt_start = 0x70090000,
			.size = 0xa000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* XUSB_PADCTL */ {
			.phys_start = 0x7009f000,
			.virt_start = 0x7009f000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMC0/1 */ {
			.phys_start = 0x700b0000,
			.virt_start = 0x700b0000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* XUSB_DEV */ {
			.phys_start = 0x700d0000,
			.virt_start = 0x700d0000,
			.size = 0xa000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HACK: CPU_DFLL clock */ {
			.phys_start = 0x70110000,
			.virt_start = 0x70110000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* USB */ {
			.phys_start = 0x7d004000,
			.virt_start = 0x7d004000,
			.size = 0x00008000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM */ {
			.phys_start = 0x80000000,
			.virt_start = 0x80000000,
			.size = 0x7bf00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0x50041000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
			},
		},
		/* GIC */ {
			.address = 0x50041000,
			.pin_base = 160,
			.pin_bitmap = {
				0xffffffff
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
