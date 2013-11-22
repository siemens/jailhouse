/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for Celsius H700, 8 GB RAM, 64 MB hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
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
	struct jailhouse_memory ALIGN mem_regions[9];
	__u8 ALIGN pio_bitmap[0x2000];
	struct jailhouse_pci_device pci_devices[25];
} ALIGN config = {
	.header = {
		.hypervisor_memory = {
			.phys_start = 0x3c000000,
			.size = 0x4000000,
		},
		.config_memory = {
			.phys_start = 0xbf7de000,
			.size = 0x21000,
		},
		.system = {
			.name = "Celsius H700",

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
			.size = 0x3c000000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE | JAILHOUSE_MEM_EXECUTE |
				JAILHOUSE_MEM_DMA,
		},
		/* RAM */ {
			.phys_start = 0x40000000,
			.virt_start = 0x40000000,
			.size = 0x7f7de000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE | JAILHOUSE_MEM_EXECUTE |
				JAILHOUSE_MEM_DMA,
		},
		/* ACPI */ {
			.phys_start = 0xbf7de000,
			.virt_start = 0xbf7de000,
			.size = 0x21000,
			.access_flags = JAILHOUSE_MEM_READ,
		},
		/* RAM */ {
			.phys_start = 0xbf7ff000,
			.virt_start = 0xbf7ff000,
			.size = 0x801000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE | JAILHOUSE_MEM_EXECUTE |
				JAILHOUSE_MEM_DMA,
		},
		/* PCI */ {
			.phys_start = 0xc0000000,
			.virt_start = 0xc0000000,
			.size = 0x3eb00000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE,
		},
		/* yeah, that's not really safe... */
		/* IOAPIC */ {
			.phys_start = 0xfec00000,
			.virt_start = 0xfec00000,
			.size = 0x1000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE,
		},
		/* the same here until we catch MSIs via interrupt remapping */
		/* HPET */ {
			.phys_start = 0xfed00000,
			.virt_start = 0xfed00000,
			.size = 0x1000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE,
		},
		/* RAM */ {
			.phys_start = 0x100000000,
			.virt_start = 0x100000000,
			.size = 0xfc000000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE | JAILHOUSE_MEM_EXECUTE |
				JAILHOUSE_MEM_DMA,
		},
		/* RAM */ {
			.phys_start = 0x200000000,
			.virt_start = 0x200000000,
			.size = 0x3c000000,
			.access_flags = JAILHOUSE_MEM_READ |
				JAILHOUSE_MEM_WRITE | JAILHOUSE_MEM_EXECUTE |
				JAILHOUSE_MEM_DMA,
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
			.devfn = 0x00,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0x08,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xb0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xb2,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xb3,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xc8,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xd0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xd8,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xe0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xe1,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xe8,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xf0,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xf8,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xfa,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xfb,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x00,
			.devfn = 0xfe,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x01,
			.devfn = 0x00,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x01,
			.devfn = 0x01,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0x10,
			.devfn = 0x00,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0xff,
			.devfn = 0x00,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0xff,
			.devfn = 0x01,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0xff,
			.devfn = 0x10,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0xff,
			.devfn = 0x11,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0xff,
			.devfn = 0x12,
		},
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bus = 0xff,
			.devfn = 0x13,
		},
	},
};
