/*
 * Jailhouse Jetson TX1 support
 *
 * Copyright (C) 2016 Evidence Srl
 *
 * Authors:
 *  Claudio Scordino <claudio@evidence.eu.com>
 *  Bruno Morelli <b.morelli@evidence.eu.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * NOTE: Add "mem=1920M vmalloc=512M" to the kernel command line.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[55];
	struct jailhouse_irqchip irqchips[2];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.hypervisor_memory = {
			.phys_start = 0xfc000000,
			.size = 0x4000000,
		},
		.debug_console = {
			.address = 0x70006000,
			.size = 0x0040,
			.flags = JAILHOUSE_CON1_TYPE_8250 |
				 JAILHOUSE_CON1_FLAG_MMIO |
				 JAILHOUSE_CON2_TYPE_ROOTPAGE,
		},
		.platform_info.arm = {
			.gicd_base = 0x50041000,
			.gicc_base = 0x50042000,
			.gich_base = 0x50044000,
			.gicv_base = 0x50046000,
			.maintenance_irq = 25,
		},
		.root_cell = {
			.name = "Jetson-TX1",
			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
		},
	},

	.cpus = {
		0xf,
	},


	.mem_regions = {

		/* PCIE */ {
			.phys_start = 0x01000000,
			.virt_start = 0x01000000,
			.size = 0x3F000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Data memory */ {
			.phys_start = 0x040000000,
			.virt_start = 0x040000000,
			.size = 0x1000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* host1x */ {
			.phys_start = 0x50000000,
			.virt_start = 0x50000000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* host1x/vi */ {
			.phys_start = 0x54080000,
			.virt_start = 0x54080000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* TSEC2 */ {
			.phys_start = 0x54100000,
			.virt_start = 0x54100000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Display 2 */ {
			.phys_start = 0x54240000,
			.virt_start = 0x54240000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* VIC */ {
			.phys_start = 0x54340000,
			.virt_start = 0x54340000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* NVJPG */ {
			.phys_start = 0x54380000,
			.virt_start = 0x54380000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* NVDEC */ {
			.phys_start = 0x54480000,
			.virt_start = 0x54480000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* NVENC */ {
			.phys_start = 0x544c0000,
			.virt_start = 0x544c0000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* TSEC */ {
			.phys_start = 0x54500000,
			.virt_start = 0x54500000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SOR1 */ {
			.phys_start = 0x54580000,
			.virt_start = 0x54580000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ISP */ {
			.phys_start = 0x54600000,
			.virt_start = 0x54600000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ISPB */ {
			.phys_start = 0x54680000,
			.virt_start = 0x54680000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* VII2C (I2C) */ {
			.phys_start = 0x546c0000,
			.virt_start = 0x546c0000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* GPU */ {
			.phys_start = 0x57000000,
			.virt_start = 0x57000000,
			.size = 0x9000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Semaphores */ {
			.phys_start = 0x60000000,
			.virt_start = 0x60000000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Legacy Interrupt Controller (ICTRL) */ {
			.phys_start = 0x60004000,
			.virt_start = 0x60004000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* TMR */ {
			.phys_start = 0x60005000,
			.virt_start = 0x60005000,
			.size = 0x01000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Clock and Reset */ {
			.phys_start = 0x60006000,
			.virt_start = 0x60006000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* Flow Controller */ {
			.phys_start = 0x60007000,
			.virt_start = 0x60007000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* System registers, secure boot, activity monitor */ {
			.phys_start = 0x6000c000,
			.virt_start = 0x6000c000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* GPIOs + exception vectors */ {
			.phys_start = 0x6000d000,
			.virt_start = 0x6000d000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MISC stuff (see datasheet) */ {
			.phys_start = 0x70000000,
			.virt_start = 0x70000000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* UARTs */ {
			.phys_start = 0x70006000,
			.virt_start = 0x70006000,
			.size = 0x600,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* PWM Controller */ {
			.phys_start = 0x7000a000,
			.virt_start = 0x7000a000,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2C  + SPI*/ {
			.phys_start = 0x7000c000,
			.virt_start = 0x7000c000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RTC  + PMC + FUSE + KFUSE */ {
			.phys_start = 0x7000e000,
			.virt_start = 0x7000e000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MC */ {
			.phys_start = 0x70019000,
			.virt_start = 0x70019000,
			.size = 0x7000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* HDA */ {
			.phys_start = 0x70030000,
			.virt_start = 0x70030000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* XUSB */ {
			.phys_start = 0x70090000,
			.virt_start = 0x70090000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SDMMCs */ {
			.phys_start = 0x700b0000,
			.virt_start = 0x700b0000,
			.size = 0x5000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SOC_THERM */ {
			.phys_start = 0x700e2000,
			.virt_start = 0x700e2000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MIPI_CAL */ {
			.phys_start = 0x700e3000,
			.virt_start = 0x700e3000,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* DVFS */ {
			.phys_start = 0x70110000,
			.virt_start = 0x70110000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ADMAIF */ {
			.phys_start = 0x702d0000,
			.virt_start = 0x702d0000,
			.size = 0x800,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* AXBAR */ {
			.phys_start = 0x702d0800,
			.virt_start = 0x702d0800,
			.size = 0x800,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* I2S */ {
			.phys_start = 0x702d1000,
			.virt_start = 0x702d1000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SFC */ {
			.phys_start = 0x702d2000,
			.virt_start = 0x702d2000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* AMX */ {
			.phys_start = 0x702d3000,
			.virt_start = 0x702d3000,
			.size = 0x800,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ADX */ {
			.phys_start = 0x702d3800,
			.virt_start = 0x702d3800,
			.size = 0x800,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* DMIC */ {
			.phys_start = 0x702d4000,
			.virt_start = 0x702d4000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* SPDIF */ {
			.phys_start = 0x702d6000,
			.virt_start = 0x702d6000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* AFC */ {
			.phys_start = 0x702d7000,
			.virt_start = 0x702d7000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* OPE1 */ {
			.phys_start = 0x702d8000,
			.virt_start = 0x702d8000,
			.size = 0x400,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* OPE2 */ {
			.phys_start = 0x702d8400,
			.virt_start = 0x702d8400,
			.size = 0x400,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MVC */ {
			.phys_start = 0x702da000,
			.virt_start = 0x702da000,
			.size = 0xc00,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MIXER */ {
			.phys_start = 0x702dbb00,
			.virt_start = 0x702dbb00,
			.size = 0x800,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ADMA */ {
			.phys_start = 0x702e2000,
			.virt_start = 0x702e2000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* AMISC */ {
			.phys_start = 0x702ec000,
			.virt_start = 0x702ec000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* ABRIDGE */ {
			.phys_start = 0x702ee000,
			.virt_start = 0x702ee000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* AAS */ {
			.phys_start = 0x702ef000,
			.virt_start = 0x702ef000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* USB */ {
			.phys_start = 0x7d000000,
			.virt_start = 0x7d000000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* 87:  System RAM */ {
			.phys_start = 0x80000000,
			.virt_start = 0x80000000,
			.size = 0x7c000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* 88:  persistent_ram */ {
			.phys_start = 0xff000000,
			.virt_start = 0xff000000,
			.size = 0x2400000,
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
				0xffffffff, 0xffffffff
			},
		},
	},
};
