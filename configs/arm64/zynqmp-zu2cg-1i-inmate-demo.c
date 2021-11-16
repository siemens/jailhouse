/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for demo inmate on Xilinx ZynqMP ZU2CG-1I:
 * 1 CPU, 64K RAM, 1 serial port
 *
 * Copyright (c) Siemens AG, 2016
 * Copyright (c) Linutronix GmbH, 2021
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Martin Kaistra <martin.kaistra@linutronix.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_cell_desc cell;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[10];
	struct jailhouse_irqchip irqchips[1];
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.name = "inmate-demo",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG | JAILHOUSE_CELL_VIRTUAL_CONSOLE_ACTIVE,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = 0,
		.num_pci_devices = 0,

		.console = {
			.address = 0xff010000,
			.type = JAILHOUSE_CON_TYPE_XUARTPS,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
	},

	.cpus = {
		0x2,
	},

	.mem_regions = {
		/* UART0 */ {
			.phys_start = 0xff000000,
			.virt_start = 0xff000000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* GEM3 */ {
			.phys_start = 0xff0e0000,
			.virt_start = 0xff0e0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C */ {
			.phys_start = 0xff020000,
			.virt_start = 0xff020000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* TTC0 - TTC3 */ {
			.phys_start = 0xff110000,
			.virt_start = 0xff110000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* IPI */ {
			.phys_start = 0xff300000,
			.virt_start = 0xff300000,
			.size = 0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* GPIO */ {
			.phys_start = 0xff0a0000,
			.virt_start = 0xff0a0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* DDR 464MB */ {
			.phys_start = 0x60000000,
			.virt_start = 0,
			.size = 0x1d000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_LOADABLE,
		},
		/* DDR shared 48MB */ {
			.phys_start = 0x7d000000,
			.virt_start = 0x7d000000,
			.size = 0x3000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_ROOTSHARED,
		},
		/* communication region */ {
			.virt_start = 0x80000000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_COMM_REGION,
		},
		/* OCM */ {
			.phys_start = 0xfffc0000,
			.virt_start = 0xfffc0000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_ROOTSHARED,
		},
	},
	.irqchips = {
		/* GIC */ {
			.address = 0xf9010000,
			.pin_base = 32,
			.pin_bitmap = {
				0,
				(1 << (67 - 64)) | /* IPI_Ch0 */
				(1 << (68 - 64)) | /* TTC0_C0 */
				(1 << (69 - 64)) | /* TTC0_C1 */
				(1 << (95 - 64)), /* gem3 */
				0,
				0
			},
		}
	}
};
