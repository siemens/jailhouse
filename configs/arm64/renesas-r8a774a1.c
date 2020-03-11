/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for HopeRun HiHope RZ/G2M platform
 * based on R8A774A1/R8A774A3
 *
 * Copyright (C) 2023 Renesas Electronics Corp.
 *
 * Authors:
 *  Lad Prabhakar <prabhakar.mahadev-lad.rj@bp.renesas.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_system header;
	struct jailhouse_cpu cpus[6];
	struct jailhouse_memory mem_regions[157];
	struct jailhouse_irqchip irqchips[4];
	struct jailhouse_pci_device pci_devices[2];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0x88000000,
			.size = 0x1000000,
		},
		.debug_console = {
			.address = 0xe6e88000,
			.size = 0x40,
			.type = JAILHOUSE_CON_TYPE_SCIF,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
		.platform_info = {
			.pci_mmconfig_base = 0xff900000,
			.pci_mmconfig_end_bus = 0,
			.pci_is_virtual = 1,
			.pci_domain = 1,
			.arm = {
				.gic_version = 2,
				.gicd_base = 0xf1010000,
				.gicc_base = 0xf102f000,
				.gich_base = 0xf1040000,
				.gicv_base = 0xf106f000,
				.maintenance_irq = 25,
			}
		},
		.root_cell = {
			.name = "Renesas HopeRun HiHope RZ/G2M",
			.num_cpus = ARRAY_SIZE(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),

			.vpci_irq_base = 12 /* SPI12 - SPI15 are reserved so use them */,
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
		/* IVSHMEM shared memory regions for 00:00.0 (demo) */
		{
			.phys_start = 0xa9000000,
			.virt_start = 0xa9000000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ,
		},
		{
			.phys_start = 0xa9001000,
			.virt_start = 0xa9001000,
			.size = 0x9000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		{
			.phys_start = 0xa900a000,
			.virt_start = 0xa900a000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		{
			.phys_start = 0xa900c000,
			.virt_start = 0xa900c000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ,
		},
		{
			.phys_start = 0xa900e000,
			.virt_start = 0xa900e000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* IVSHMEM shared memory regions for 00:01.0 (networking) */
		JAILHOUSE_SHMEM_NET_REGIONS(0xa9010000, 0),
		/* RAM */ {
			.phys_start = 0x48000000,
			.virt_start = 0x48000000,
			.size = 0x40000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_EXECUTE,
		},
		/* Leave hole for hypervisor */
		/* RAM - reserved for baremetal apps */ {
			.phys_start = 0x89000000,
			.virt_start = 0x89000000,
			.size = 0x6400000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_LOADABLE,
		},
		/* RAM - reserved for inmate */ {
			.phys_start = 0x8f400000,
			.virt_start = 0x8f400000,
			.size = 0x19c00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_LOADABLE,
		},
		/* RAM */ {
			.phys_start = 0xa9400000,
			.virt_start = 0xa9400000,
			.size = 0x16c00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_EXECUTE,
		},
		/* RAM */ {
			.phys_start = 0x600000000,
			.virt_start = 0x600000000,
			.size = 0x80000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_EXECUTE,
		},
		/* QOS */ {
			.phys_start = 0xe67e0000,
			.virt_start = 0xe67e0000,
			.size = 0x10090,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* RWDT */ {
			.phys_start = 0xe6020000,
			.virt_start = 0xe6020000,
			.size = 0x0c,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GPIO0 */ {
			.phys_start = 0xe6050000,
			.virt_start = 0xe6050000,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GPIO1 */ {
			.phys_start = 0xe6051000,
			.virt_start = 0xe6051000,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GPIO2 */ {
			.phys_start = 0xe6052000,
			.virt_start = 0xe6052000,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GPIO3 */ {
			.phys_start = 0xe6053000,
			.virt_start = 0xe6053000,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GPIO4 */ {
			.phys_start = 0xe6054000,
			.virt_start = 0xe6054000,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GPIO5 */ {
			.phys_start = 0xe6055000,
			.virt_start = 0xe6055000,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GPIO6 */ {
			.phys_start = 0xe6055400,
			.virt_start = 0xe6055400,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GPIO7 */ {
			.phys_start = 0xe6055800,
			.virt_start = 0xe6055800,
			.size = 0x50,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PFC */ {
			.phys_start = 0xe6060000,
			.virt_start = 0xe6060000,
			.size = 0x50c,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CMT0 */ {
			.phys_start = 0xe60f0000,
			.virt_start = 0xe60f0000,
			.size = 0x1004,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CMT1 */ {
			.phys_start = 0xe6130000,
			.virt_start = 0xe6130000,
			.size = 0x1004,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CMT2 */ {
			.phys_start = 0xe6140000,
			.virt_start = 0xe6140000,
			.size = 0x1004,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CMT3 */ {
			.phys_start = 0xe6148000,
			.virt_start = 0xe6148000,
			.size = 0x1004,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CPG */ {
			.phys_start = 0xe6150000,
			.virt_start = 0xe6150000,
			.size = 0x0bb0,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* RST */ {
			.phys_start = 0xe6160000,
			.virt_start = 0xe6160000,
			.size = 0x018c,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SYSC */ {
			.phys_start = 0xe6180000,
			.virt_start = 0xe6180000,
			.size = 0x0400,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TSC */ {
			.phys_start = 0xe6198000,
			.virt_start = 0xe6198000,
			.size = 0x0100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TSC */ {
			.phys_start = 0xe61a0000,
			.virt_start = 0xe61a0000,
			.size = 0x0100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TSC */ {
			.phys_start = 0xe61a8000,
			.virt_start = 0xe61a8000,
			.size = 0x0100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* INTC EX */ {
			.phys_start = 0xe61c0000,
			.virt_start = 0xe61c0000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TMU0 */ {
			.phys_start = 0xe61e0000,
			.virt_start = 0xe61e0000,
			.size = 0x30,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TMU1 */ {
			.phys_start = 0xe6fc0000,
			.virt_start = 0xe6fc0000,
			.size = 0x30,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TMU2 */ {
			.phys_start = 0xe6fd0000,
			.virt_start = 0xe6fd0000,
			.size = 0x30,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TMU3 */ {
			.phys_start = 0xe6fe0000,
			.virt_start = 0xe6fe0000,
			.size = 0x30,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TMU4 */ {
			.phys_start = 0xffc00000,
			.virt_start = 0xffc00000,
			.size = 0x30,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* I2C0 */ {
			.phys_start = 0xe6500000,
			.virt_start = 0xe6500000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* I2C1 */ {
			.phys_start = 0xe6508000,
			.virt_start = 0xe6508000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* I2C2 */ {
			.phys_start = 0xe6510000,
			.virt_start = 0xe6510000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* I2C3 */ {
			.phys_start = 0xe66d0000,
			.virt_start = 0xe66d0000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* I2C4 */ {
			.phys_start = 0xe66d8000,
			.virt_start = 0xe66d8000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* I2C5 */ {
			.phys_start = 0xe66e0000,
			.virt_start = 0xe66e0000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* I2C6 */ {
			.phys_start = 0xe66e8000,
			.virt_start = 0xe66e8000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* I2C DVFS */ {
			.phys_start = 0xe60b0000,
			.virt_start = 0xe60b0000,
			.size = 0x425,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* HSCIF0 */ {
			.phys_start = 0xe6540000,
			.virt_start = 0xe6540000,
			.size = 0x60,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* HSCIF1 */ {
			.phys_start = 0xe6550000,
			.virt_start = 0xe6550000,
			.size = 0x60,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* HSCIF2 */ {
			.phys_start = 0xe6560000,
			.virt_start = 0xe6560000,
			.size = 0x60,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* HSCIF3 */ {
			.phys_start = 0xe66a0000,
			.virt_start = 0xe66a0000,
			.size = 0x60,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* HSCIF4 */ {
			.phys_start = 0xe66b0000,
			.virt_start = 0xe66b0000,
			.size = 0x60,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* HSUSB */ {
			.phys_start = 0xe6590000,
			.virt_start = 0xe6590000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* USB2 CLKSEL */ {
			.phys_start = 0xe6590630,
			.virt_start = 0xe6590630,
			.size = 0x2,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_32 |JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* USB DMAC0 */ {
			.phys_start = 0xe65a0000,
			.virt_start = 0xe65a0000,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* USB DMAC1 */ {
			.phys_start = 0xe65b0000,
			.virt_start = 0xe65b0000,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* USB3 PHY */ {
			.phys_start = 0xe65ee000,
			.virt_start = 0xe65ee000,
			.size = 0x90,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* DMAC0 */ {
			.phys_start = 0xe6700000,
			.virt_start = 0xe6700000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* DMAC1 */ {
			.phys_start = 0xe7300000,
			.virt_start = 0xe7300000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* DMAC2 */ {
			.phys_start = 0xe7310000,
			.virt_start = 0xe7310000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU DS0 */ {
			.phys_start = 0xe6740000,
			.virt_start = 0xe6740000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU DS1 */ {
			.phys_start = 0xe7740000,
			.virt_start = 0xe7740000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU HC */ {
			.phys_start = 0xe6570000,
			.virt_start = 0xe6570000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU MM */ {
			.phys_start = 0xe67b0000,
			.virt_start = 0xe67b0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU MP */ {
			.phys_start = 0xec670000,
			.virt_start = 0xec670000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU PV0 */ {
			.phys_start = 0xfd800000,
			.virt_start = 0xfd800000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU PV1 */ {
			.phys_start = 0xfd950000,
			.virt_start = 0xfd950000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU VC0 */ {
			.phys_start = 0xfe6b0000,
			.virt_start = 0xfe6b0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IPMMU VI0 */ {
			.phys_start = 0xfebd0000,
			.virt_start = 0xfebd0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* AVB */ {
			.phys_start = 0xe6800000,
			.virt_start = 0xe6800000,
			.size = 0x800,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CAN0 */ {
			.phys_start = 0xe6c30000,
			.virt_start = 0xe6c30000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CAN1 */ {
			.phys_start = 0xe6c38000,
			.virt_start = 0xe6c38000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CANFD */ {
			.phys_start = 0xe66c0000,
			.virt_start = 0xe66c0000,
			.size = 0x8000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PWM0 */ {
			.phys_start = 0xe6e30000,
			.virt_start = 0xe6e30000,
			.size = 0x8,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PWM1 */ {
			.phys_start = 0xe6e31000,
			.virt_start = 0xe6e31000,
			.size = 0x8,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PWM2 */ {
			.phys_start = 0xe6e32000,
			.virt_start = 0xe6e32000,
			.size = 0x8,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PWM3 */ {
			.phys_start = 0xe6e33000,
			.virt_start = 0xe6e33000,
			.size = 0x8,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PWM4 */ {
			.phys_start = 0xe6e34000,
			.virt_start = 0xe6e34000,
			.size = 0x8,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PWM5 */ {
			.phys_start = 0xe6e35000,
			.virt_start = 0xe6e35000,
			.size = 0x8,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PWM6 */ {
			.phys_start = 0xe6e36000,
			.virt_start = 0xe6e36000,
			.size = 0x8,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* TPU */ {
			.phys_start = 0xe6e80000,
			.virt_start = 0xe6e80000,
			.size = 0x148,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SCIF0 */ {
			.phys_start = 0xe6e60000,
			.virt_start = 0xe6e60000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SCIF1 */ {
			.phys_start = 0xe6e68000,
			.virt_start = 0xe6e68000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SCIF2 */ {
			.phys_start = 0xe6e88000,
			.virt_start = 0xe6e88000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SCIF3 */ {
			.phys_start = 0xe6c50000,
			.virt_start = 0xe6c50000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SCIF4 */ {
			.phys_start = 0xe6c40000,
			.virt_start = 0xe6c40000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SCIF5 */ {
			.phys_start = 0xe6f30000,
			.virt_start = 0xe6f30000,
			.size = 0x40,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_8 | JAILHOUSE_MEM_IO_16 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* MSIOF0 */ {
			.phys_start = 0xe6e90000,
			.virt_start = 0xe6e90000,
			.size = 0x64,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* MSIOF1 */ {
			.phys_start = 0xe6ea0000,
			.virt_start = 0xe6ea0000,
			.size = 0x64,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* MSIOF2 */ {
			.phys_start = 0xe6c00000,
			.virt_start = 0xe6c00000,
			.size = 0x64,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* MSIOF3 */ {
			.phys_start = 0xe6c10000,
			.virt_start = 0xe6c10000,
			.size = 0x64,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VIN0 */ {
			.phys_start = 0xe6ef0000,
			.virt_start = 0xe6ef0000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VIN1 */ {
			.phys_start = 0xe6ef1000,
			.virt_start = 0xe6ef1000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VIN2 */ {
			.phys_start = 0xe6ef2000,
			.virt_start = 0xe6ef2000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VIN3 */ {
			.phys_start = 0xe6ef3000,
			.virt_start = 0xe6ef3000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VIN4 */ {
			.phys_start = 0xe6ef4000,
			.virt_start = 0xe6ef4000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VIN5 */ {
			.phys_start = 0xe6ef5000,
			.virt_start = 0xe6ef5000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VIN6 */ {
			.phys_start = 0xe6ef6000,
			.virt_start = 0xe6ef6000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VIN7 */ {
			.phys_start = 0xe6ef7000,
			.virt_start = 0xe6ef7000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SOUND */ {
			.phys_start = 0xec500000,
			.virt_start = 0xec500000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SOUND */ {
			.phys_start = 0xec5a0000,
			.virt_start = 0xec5a0000,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SOUND */ {
			.phys_start = 0xec540000,
			.virt_start = 0xec540000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SOUND */ {
			.phys_start = 0xec541000,
			.virt_start = 0xec541000,
			.size = 0x280,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SOUND */ {
			.phys_start = 0xec760000,
			.virt_start = 0xec760000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* AUDMA0 */ {
			.phys_start = 0xec700000,
			.virt_start = 0xec700000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* AUDMA1 */ {
			.phys_start = 0xec720000,
			.virt_start = 0xec720000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_16 | JAILHOUSE_MEM_IO_32 |
				 JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* XHCI0 */ {
			.phys_start = 0xee000000,
			.virt_start = 0xee000000,
			.size = 0xc00,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* USB3 PERI */ {
			.phys_start = 0xee020000,
			.virt_start = 0xee020000,
			.size = 0x400,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* OHCI0 */ {
			.phys_start = 0xee080000,
			.virt_start = 0xee080000,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* OHCI1 */ {
			.phys_start = 0xee0a0000,
			.virt_start = 0xee0a0000,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* EHCI0 */ {
			.phys_start = 0xee080100,
			.virt_start = 0xee080100,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* EHCI1 */ {
			.phys_start = 0xee0a0100,
			.virt_start = 0xee0a0100,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* USB2 PHY0 */ {
			.phys_start = 0xee080200,
			.virt_start = 0xee080200,
			.size = 0x700,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* USB2 PHY1 */ {
			.phys_start = 0xee0a0200,
			.virt_start = 0xee0a0200,
			.size = 0x700,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SDHI0 */ {
			.phys_start = 0xee100000,
			.virt_start = 0xee100000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SDHI1 */ {
			.phys_start = 0xee120000,
			.virt_start = 0xee120000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SDHI2 */ {
			.phys_start = 0xee140000,
			.virt_start = 0xee140000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* SDHI3 */ {
			.phys_start = 0xee160000,
			.virt_start = 0xee160000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* GSX */ {
			.phys_start = 0xfd000000,
			.virt_start = 0xfd000000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI0 */ {
			.phys_start = 0xfe000000,
			.virt_start = 0xfe000000,
			.size = 0x80000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI0 */ {
			.phys_start = 0xfe100000,
			.virt_start = 0xfe100000,
			.size = 0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI0 */ {
			.phys_start = 0xfe200000,
			.virt_start = 0xfe200000,
			.size = 0x200000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI0 */ {
			.phys_start = 0x30000000,
			.virt_start = 0x30000000,
			.size = 0x8000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI0 */ {
			.phys_start = 0x38000000,
			.virt_start = 0x38000000,
			.size = 0x8000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI1 */ {
			.phys_start = 0xee800000,
			.virt_start = 0xee800000,
			.size = 0x80000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI1 */ {
			.phys_start = 0xee900000,
			.virt_start = 0xee900000,
			.size = 0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI1 */ {
			.phys_start = 0xeea00000,
			.virt_start = 0xeea00000,
			.size = 0x200000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI1 */ {
			.phys_start = 0xc0000000,
			.virt_start = 0xc0000000,
			.size = 0x8000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PCI1 */ {
			.phys_start = 0xc8000000,
			.virt_start = 0xc8000000,
			.size = 0x8000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* FCPCI0 */ {
			.phys_start = 0xfe8df000,
			.virt_start = 0xfe8df000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* FCPCS */ {
			.phys_start = 0xfe90f000,
			.virt_start = 0xfe90f000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IVDP1C */ {
			.phys_start = 0xfe8d0000,
			.virt_start = 0xfe8d0000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* IVDP1C */ {
			.phys_start = 0xfe8d0200,
			.virt_start = 0xfe8d0200,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VCPLF */ {
			.phys_start = 0xfe910000,
			.virt_start = 0xfe910000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VCPLF */ {
			.phys_start = 0xfe910200,
			.virt_start = 0xfe910200,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VDPB */ {
			.phys_start = 0xfe900000,
			.virt_start = 0xfe900000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VDPB */ {
			.phys_start = 0xfe900200,
			.virt_start = 0xfe900200,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* FDP0 */ {
			.phys_start = 0xfe940000,
			.virt_start = 0xfe940000,
			.size = 0x2400,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* FDP0 */ {
			.phys_start = 0xfe950000,
			.virt_start = 0xfe950000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* FCPVD0 */ {
			.phys_start = 0xfea27000,
			.virt_start = 0xfea27000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* FCPVD1 */ {
			.phys_start = 0xfea2f000,
			.virt_start = 0xfea2f000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* FCPVD2 */ {
			.phys_start = 0xfea37000,
			.virt_start = 0xfea37000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VSP */ {
			.phys_start = 0xfe960000,
			.virt_start = 0xfe960000,
			.size = 0x8000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VSP-B */ {
			.phys_start = 0xfe96f000,
			.virt_start = 0xfe96f000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VSPD0 */ {
			.phys_start = 0xfea20000,
			.virt_start = 0xfea20000,
			.size = 0x5000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VSPD1 */ {
			.phys_start = 0xfea28000,
			.virt_start = 0xfea28000,
			.size = 0x5000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VSPD2 */ {
			.phys_start = 0xfea30000,
			.virt_start = 0xfea30000,
			.size = 0x5000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VSP */ {
			.phys_start = 0xfe9a0000,
			.virt_start = 0xfe9a0000,
			.size = 0x8000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* VSPI0 */ {
			.phys_start = 0xfe9af000,
			.virt_start = 0xfe9af000,
			.size = 0x200,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CSI20 */ {
			.phys_start = 0xfea80000,
			.virt_start = 0xfea80000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* CSI40 */ {
			.phys_start = 0xfeaa0000,
			.virt_start = 0xfeaa0000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* HDMI0 */ {
			.phys_start = 0xfead0000,
			.virt_start = 0xfead0000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* DU */ {
			.phys_start = 0xfeb00000,
			.virt_start = 0xfeb00000,
			.size = 0x70000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* LVDS */ {
			.phys_start = 0xfeb90000,
			.virt_start = 0xfeb90000,
			.size = 0x14,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
		/* PRR */ {
			.phys_start = 0xfff00044,
			.virt_start = 0xfff00044,
			.size = 0x4,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				 JAILHOUSE_MEM_IO_32 | JAILHOUSE_MEM_IO_UNALIGNED,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0xf1010000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0xf1010000,
			.pin_base = 160,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0xf1010000,
			.pin_base = 288,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0xf1010000,
			.pin_base = 416,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
	},

	.pci_devices = {
		{ /* IVSHMEM 0001:00:00.0 (demo) */
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 1,
			.bdf = 0 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 0,
			.shmem_dev_id = 0,
			.shmem_peers = 3,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_UNDEFINED,
		},
		{ /* IVSHMEM 0001:00:01.0 (networking) */
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 1,
			.bdf = 1 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 5,
			.shmem_dev_id = 0,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},

};
