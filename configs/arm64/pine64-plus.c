/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for Pine64+ board, 2 GB
 *
 * Copyright (c) Vijai Kumar K, 2019-2020
 *
 * Authors:
 *  Vijai Kumar K <vijaikumar.kanagarajan@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * NOTE: Add "mem=1792M" to the kernel command line.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[43];
	struct jailhouse_irqchip irqchips[1];
	struct jailhouse_pci_device pci_devices[2];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0xbc000000,
			.size =       0x04000000,
		},
		.debug_console = {
			.address = 0x01c28000,
			.size = 0x400,
			.type = JAILHOUSE_CON_TYPE_8250,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
		.platform_info = {
			.pci_mmconfig_base = 0x02000000,
			.pci_mmconfig_end_bus = 0,
			.pci_is_virtual = 1,
			.arm = {
				.gic_version = 2,
				.gicd_base = 0x01c81000,
				.gicc_base = 0x01c82000,
				.gich_base = 0x01c84000,
				.gicv_base = 0x01c86000,
				.maintenance_irq = 25,
			},
		},
		.root_cell = {
			.name = "Pine64-Plus",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),
			.num_irqchips = ARRAY_SIZE(config.irqchips),

			.vpci_irq_base = 108,
		},
	},

	.cpus = {
		0xf,
	},

	.mem_regions = {
		/* IVSHMEM shared memory regions for 00:00.0 (demo) */
		/* State Table */ {
			.phys_start = 0xbbef1000,
			.virt_start = 0xbbef1000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* Read/Write Section */ {
			.phys_start = 0xbbef2000,
			.virt_start = 0xbbef2000,
			.size = 0x9000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* Output (peer 0) */ {
			.phys_start = 0xbbefb000,
			.virt_start = 0xbbefb000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* Output (peer 1) */ {
			.phys_start = 0xbbefd000,
			.virt_start = 0xbbefd000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* Output (peer 2) */ {
			.phys_start = 0xbbeff000,
			.virt_start = 0xbbeff000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ,
		},
		 /* IVSHMEM shared memory region for 00:01.0 (networking)*/
                JAILHOUSE_SHMEM_NET_REGIONS(0xbbf01000, 0),
                /* SRAM */ {
                        .phys_start = 0x00018000,
                        .virt_start = 0x00018000,
                        .size =       0x00028000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_EXECUTE,
                },
                /* Clock */ {
                        .phys_start = 0x01000000,
                        .virt_start = 0x01000000,
                        .size =       0x00100000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* 1100000.mixer */ {
                        .phys_start = 0x01100000,
                        .virt_start = 0x01100000,
                        .size =       0x00100000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* 1200000.mixer */ {
                        .phys_start = 0x01200000,
                        .virt_start = 0x01200000,
                        .size =       0x00100000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* Syscon */ {
                        .phys_start = 0x01c00000,
                        .virt_start = 0x01c00000,
                        .size =       0x00001000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* DMA */ {
                        .phys_start = 0x01c02000,
                        .virt_start = 0x01c02000,
                        .size =       0x00001000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* LCD1 */ {
                        .phys_start = 0x01c0c000,
                        .virt_start = 0x01c0c000,
                        .size =       0x00001000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* LCD2 */ {
                        .phys_start = 0x01c0d000,
                        .virt_start = 0x01c0d000,
                        .size =       0x00001000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* MMC */ {
                        .phys_start = 0x01c0f000,
                        .virt_start = 0x01c0f000,
                        .size =       0x00001000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* EEPROM */ {
                        .phys_start = 0x01c14000,
                        .virt_start = 0x01c14000,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* USB */ {
                        .phys_start = 0x01c19000,
                        .virt_start = 0x01c19000,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* USB */ {
                        .phys_start = 0x01c19400,
                        .virt_start = 0x01c19400,
                        .size =       0x00000014,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* USB */ {
                        .phys_start = 0x01c1a000,
                        .virt_start = 0x01c1a000,
                        .size =       0x00000100,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* USB */ {
                        .phys_start = 0x01c1a400,
                        .virt_start = 0x01c1a400,
                        .size =       0x00000100,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* USB */ {
                        .phys_start = 0x01c1a800,
                        .virt_start = 0x01c1a800,
                        .size =       0x00000100,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* USB */ {
                        .phys_start = 0x01c1b000,
                        .virt_start = 0x01c1b000,
                        .size =       0x00000100,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* USB */ {
                        .phys_start = 0x01c1b400,
                        .virt_start = 0x01c1b400,
                        .size =       0x00000100,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* USB */ {
                        .phys_start = 0x01c1b800,
                        .virt_start = 0x01c1b800,
                        .size =       0x00000004,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* Clock */ {
                        .phys_start = 0x01c20000,
                        .virt_start = 0x01c20000,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* Pincontrol */ {
                        .phys_start = 0x01c20800,
                        .virt_start = 0x01c20800,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* Watchdog */ {
                        .phys_start = 0x01c20ca0,
                        .virt_start = 0x01c20ca0,
                        .size =       0x00000020,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* UART */ {
                        .phys_start = 0x01c28000,
                        .virt_start = 0x01c28000,
                        .size =       0x00000020,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* I2C */ {
                        .phys_start = 0x01c2b000,
                        .virt_start = 0x01c2b000,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* Ethernet */ {
                        .phys_start = 0x01c30000,
                        .virt_start = 0x01c30000,
                        .size =       0x00010000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* GPU */ {
                        .phys_start = 0x01c40000,
                        .virt_start = 0x01c40000,
                        .size =       0x00010000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* SRAM */ {
                        .phys_start = 0x01d00000,
                        .virt_start = 0x01d00000,
                        .size =       0x00040000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_EXECUTE,
                },
                /* HDMI */ {
                        .phys_start = 0x01ee0000,
                        .virt_start = 0x01ee0000,
                        .size =       0x00010000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* HDMI */ {
                        .phys_start = 0x01ef0000,
                        .virt_start = 0x01ef0000,
                        .size =       0x00010000,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
                /* RTC */ {
                        .phys_start = 0x01f00000,
                        .virt_start = 0x01f00000,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* Interrupt Controller */ {
                        .phys_start = 0x01f00c00,
                        .virt_start = 0x01f00c00,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* Clock */ {
                        .phys_start = 0x01f01400,
                        .virt_start = 0x01f01400,
                        .size =       0x00000100,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* Pincontrol */ {
                        .phys_start = 0x01f02c00,
                        .virt_start = 0x01f02c00,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
                },
                /* RSB(Reduced Serial Bus) */ {
                        .phys_start = 0x01f03400,
                        .virt_start = 0x01f03400,
                        .size =       0x00000400,
                        .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                                JAILHOUSE_MEM_IO,
                },
		/* System RAM */ {
			.phys_start = 0x40000000,
			.virt_start = 0x40000000,
			.size = 0x7bef1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0x01c81000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
	},

	.pci_devices = {
		{ /* IVSHMEM 00:00.0 (demo) */
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 0 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 0,
			.shmem_dev_id = 0,
			.shmem_peers = 3,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_UNDEFINED,
		},
		/* IVSHMEM 00:01.0 (networking) */ {
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.bdf = 1 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 5,
			.shmem_dev_id = 0,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
