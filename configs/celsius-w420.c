/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for Celsius W420, 4 GB RAM, 64 MB hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Ivan Kolchin <ivan.kolchin@siemens.com>
 * 
 * NOTE: This config expects the hypervisor to be at 0x1bf00000
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/types.h>
#include <jailhouse/cell-config.h>

#define ALIGN __attribute__((aligned(1)))
#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_system ALIGN header;
	__u64 ALIGN cpus[1];
	struct jailhouse_memory ALIGN mem_regions[13];
	__u8 ALIGN pio_bitmap[0x2000];
	struct jailhouse_pci_device pci_devices[13];
} ALIGN config = {
	.header = {
		.hypervisor_memory = {
			.phys_start = 0x1c000000,
			.size = 0x4000000,
		},
		.config_memory = {
			.phys_start = 0xd8a1a000,
			.size = 0x10000,
		},
		.system = {
			.name = "Celsius-W420",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irq_lines = 0,
			.pio_bitmap_size = ARRAY_SIZE(config.pio_bitmap),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),
		},
	},

	.cpus = {
		0xf,
	},

	.mem_regions = {
		/* RAM */ {
			.phys_start = 0x0,
			.virt_start = 0x0,
			.size = 0x1bf00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* RAM */ {
			.phys_start = 0x1ff00000,
			.virt_start = 0x1ff00000,
			.size = 0x100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* RAM */ {
			.phys_start = 0x20200000,
			.virt_start = 0x20200000,
			.size = 0x1fe04000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* RAM */ {
			.phys_start = 0x40005000,
			.virt_start = 0x40005000,
			.size = 0x97f3c000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* ACPI */ {
			.phys_start = 0xd8a2a000,
			.virt_start = 0xd8a2a000,
			.size = 0x11e000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* RAM */ {
			.phys_start = 0xda382000,
			.virt_start = 0xda382000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* ACPI */ {
			.phys_start = 0xda383000,
			.virt_start = 0xda383000,
			.size = 0x43000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* RAM */ {
			.phys_start = 0xda3c6000,
			.virt_start = 0xda3c6000,
			.size = 0x9df000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* RAM */ {
			.phys_start = 0xdafef000,
			.virt_start = 0xdafef000,
			.size = 0x11000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* PCI */ {
			.phys_start = 0xdfa00000,
			.virt_start = 0xdfa00000,
			.size = 0x1f100000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* yeah, that's not really safe... */
		/* IOAPIC */ {
			.phys_start = 0xfec00000,
			.virt_start = 0xfec00000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* the same here until we catch MSIs via interrupt remapping */
		/* HPET */ {
			.phys_start = 0xfed00000,
			.virt_start = 0xfed00000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* RAM */ {
			.phys_start = 0x100000000,
			.virt_start = 0x100000000,
			.size = 0x1e600000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
	},

	.pio_bitmap = {
		[     0/8 ...   0x1f/8] = -1,
		[  0x20/8 ...   0x27/8] = 0xfc, /* HACK: PIC */
		[  0x28/8 ...   0x3f/8] = -1,
		[  0x40/8 ...   0x47/8] = 0xf0, /* PIT */
		[  0x48/8 ...   0x5f/8] = -1,
		[  0x60/8 ...   0x67/8] = 0x0, /* HACK: 8042, and more? */
		[  0x68/8 ...   0x6f/8] = -1,
		[  0x70/8 ...   0x77/8] = 0xfc, /* rtc */
		[  0x78/8 ...   0x7f/8] = -1,
		[  0x80/8 ...   0x8f/8] = 0, /* dma */
		[  0x90/8 ...  0x16f/8] = -1,
		[ 0x170/8 ...  0x177/8] = 0, /* ide */
		[ 0x178/8 ...  0x1ef/8] = -1,
		[ 0x1f0/8 ...  0x1f7/8] = 0, /* ide */
		[ 0x1f8/8 ...  0x2f7/8] = -1,
		[ 0x2f8/8 ...  0x2ff/8] = 0, /* serial2 */
		[ 0x300/8 ...  0x36f/8] = -1,
		[ 0x370/8 ...  0x377/8] = 0xbf, /* ide */
		[ 0x378/8 ...  0x3af/8] = -1,
		[ 0x3b0/8 ...  0x3df/8] = 0, /* VGA */
		[ 0x3e0/8 ...  0x3f7/8] = -1,
		[ 0x3f8/8 ...  0x3ff/8] = 0, /* serial 1 */
		[ 0x400/8 ...  0x47f/8] = 0, /* ACPI...? */
		[ 0x480/8 ...  0xcf7/8] = -1,
		[ 0xcf8/8 ... 0xffff/8] = 0, /* HACK: full PCI */
	},

	.pci_devices = {
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x00<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x02<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x14<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x16<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x19<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x1a<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x1b<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x1d<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x1e<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x1f<<3)|0x0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x1f<<3)|0x2,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x1f<<3)|0x3,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = (0x1f<<3)|0x6,
		},
	},
};
