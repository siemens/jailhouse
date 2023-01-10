/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for ASRock IMB-A180 G-Series (4G RAM) board
 * created with 'jailhouse config create imb-a180.c'
 *
 * Copyright (c) Siemens AG, 2014
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Adjusted by Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * NOTE: This config expects the following to be appended to your kernel cmdline
 *       "memmap=82M$0x3a000000"
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[42];
	struct jailhouse_irqchip irqchips[1];
	struct jailhouse_pio pio_regions[8];
	struct jailhouse_pci_device pci_devices[26];
	struct jailhouse_pci_capability pci_caps[26];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_X86,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0x3a000000,
			.size = 0x600000,
		},
		.debug_console = {
			.address = 0x3f8,
			/* .divider = 0x1, */
			.type = JAILHOUSE_CON_TYPE_8250,
			.flags = JAILHOUSE_CON_ACCESS_PIO,
		},
		.platform_info = {
			.pci_mmconfig_base = 0xe0000000,
			.pci_mmconfig_end_bus = 0xff,
			.x86 = {
				.pm_timer_address = 0x808,
			},
		},
		.root_cell = {
			.name = "IMB-A180",
			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.num_pio_regions = ARRAY_SIZE(config.pio_regions),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),
			.num_pci_caps = ARRAY_SIZE(config.pci_caps),
		},
	},

	.cpus = {
		0x000000000000000f,
	},

	.mem_regions = {
		/* MemRegion: 00000000-0009e7ff : System RAM */
		{
			.phys_start = 0x0,
			.virt_start = 0x0,
			.size = 0x9f000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 000a0000-000bffff : PCI Bus 0000:00 */
		{
			.phys_start = 0xa0000,
			.virt_start = 0xa0000,
			.size = 0x20000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: 000c0000-000ce9ff : Video ROM */
		{
			.phys_start = 0xc0000,
			.virt_start = 0xc0000,
			.size = 0xf000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* MemRegion: 000e0000-000fffff : System ROM */
		{
			.phys_start = 0xe0000,
			.virt_start = 0xe0000,
			.size = 0x20000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* MemRegion: 00100000-3affffff : System RAM */
		{
			.phys_start = 0x00100000,
			.virt_start = 0x00100000,
			.size = 0x3af00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 3f200000-9db10fff : System RAM */
		{
			.phys_start = 0x3f200000,
			.virt_start = 0x3f200000,
			.size = 0x5e911000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 9db41000-9dc7ffff : System RAM */
		{
			.phys_start = 0x9db41000,
			.virt_start = 0x9db41000,
			.size = 0x13f000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 9dc80000-9e148fff : ACPI Non-volatile Storage */
		{
			.phys_start = 0x9dc80000,
			.virt_start = 0x9dc80000,
			.size = 0x4c9000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* MemRegion: 9ede5000-9ede5fff : System RAM */
		{
			.phys_start = 0x9ede5000,
			.virt_start = 0x9ede5000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 9ede6000-9ededfff : ACPI Non-volatile Storage */
		{
			.phys_start = 0x9ede6000,
			.virt_start = 0x9ede6000,
			.size = 0x8000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* MemRegion: 9edee000-9ef42fff : System RAM */
		{
			.phys_start = 0x9edee000,
			.virt_start = 0x9edee000,
			.size = 0x155000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 9f42d000-9f46ffff : System RAM */
		{
			.phys_start = 0x9f42d000,
			.virt_start = 0x9f42d000,
			.size = 0x43000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 9f7f1000-9f7fffff : System RAM */
		{
			.phys_start = 0x9f7f1000,
			.virt_start = 0x9f7f1000,
			.size = 0xf000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 9f800000-9fffffff : RAM buffer */
		{
			.phys_start = 0x9f800000,
			.virt_start = 0x9f800000,
			.size = 0x800000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: a0000000-bfffffff : pnp 00:01 */
		{
			.phys_start = 0xa0000000,
			.virt_start = 0xa0000000,
			.size = 0x20000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: c0000000-cfffffff : 0000:00:01.0 */
		{
			.phys_start = 0xc0000000,
			.virt_start = 0xc0000000,
			.size = 0x10000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: d0000000-d07fffff : 0000:00:01.0 */
		{
			.phys_start = 0xd0000000,
			.virt_start = 0xd0000000,
			.size = 0x800000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: d0800000-d0803fff : r8169 */
		{
			.phys_start = 0xd0800000,
			.virt_start = 0xd0800000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: d0804000-d0804fff : r8169 */
		{
			.phys_start = 0xd0804000,
			.virt_start = 0xd0804000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: d0900000-d0903fff : r8169 */
		{
			.phys_start = 0xd0900000,
			.virt_start = 0xd0900000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fea00000-fea03fff : 0000:01:00.3 */
		{
			.phys_start = 0xfea00000,
			.virt_start = 0xfea00000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fea04000-fea07fff : 0000:01:00.2 */
		{
			.phys_start = 0xfea04000,
			.virt_start = 0xfea04000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fea08000-fea0bfff : 0000:01:00.1 */
		{
			.phys_start = 0xfea08000,
			.virt_start = 0xfea08000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fea0c000-fea0c0ff : 0000:01:00.3 */
		{
			.phys_start = 0xfea0c000,
			.virt_start = 0xfea0c000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fea0d000-fea0dfff : 0000:01:00.2 */
		{
			.phys_start = 0xfea0d000,
			.virt_start = 0xfea0d000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fea0e000-fea0efff : 0000:01:00.1 */
		{
			.phys_start = 0xfea0e000,
			.virt_start = 0xfea0e000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fea0f000-fea0ffff : r8169 */
		{
			.phys_start = 0xfea0f000,
			.virt_start = 0xfea0f000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb00000-feb3ffff : 0000:00:01.0 */
		{
			.phys_start = 0xfeb00000,
			.virt_start = 0xfeb00000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb40000-feb5ffff : 0000:00:01.0 */
		{
			.phys_start = 0xfeb40000,
			.virt_start = 0xfeb40000,
			.size = 0x20000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb60000-feb63fff : ICH HD audio */
		{
			.phys_start = 0xfeb60000,
			.virt_start = 0xfeb60000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb64000-feb67fff : ICH HD audio */
		{
			.phys_start = 0xfeb64000,
			.virt_start = 0xfeb64000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb68000-feb69fff : xhci_hcd */
		{
			.phys_start = 0xfeb68000,
			.virt_start = 0xfeb68000,
			.size = 0x2000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6a000-feb6a0ff : ehci_hcd */
		{
			.phys_start = 0xfeb6a000,
			.virt_start = 0xfeb6a000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6b000-feb6bfff : ohci_hcd */
		{
			.phys_start = 0xfeb6b000,
			.virt_start = 0xfeb6b000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6c000-feb6c0ff : ehci_hcd */
		{
			.phys_start = 0xfeb6c000,
			.virt_start = 0xfeb6c000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6d000-feb6dfff : ohci_hcd */
		{
			.phys_start = 0xfeb6d000,
			.virt_start = 0xfeb6d000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6e000-feb6e3ff : ahci */
		{
			.phys_start = 0xfeb6e000,
			.virt_start = 0xfeb6e000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fed00000-fed003ff : HPET 0 */
		{
			.phys_start = 0xfed00000,
			.virt_start = 0xfed00000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fed61000-fed70fff : pnp 00:0e */
		{
			.phys_start = 0xfed61000,
			.virt_start = 0xfed61000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: 100001000-13effffff : System RAM */
		{
			.phys_start = 0x100001000,
			.virt_start = 0x100001000,
			.size = 0x3efff000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 13f000000-13fffffff : RAM buffer */
		{
			.phys_start = 0x13f000000,
			.virt_start = 0x13f000000,
			.size = 0x1000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 3f000000-3f1fffff : JAILHOUSE Inmate Memory */
		{
			.phys_start = 0x3f000000,
			.virt_start = 0x3f000000,
			.size = 0x200000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
	},

	.irqchips = {
		/* IOAPIC */ {
			.address = 0xfec00000,
			.id = 0x0,
			.pin_bitmap = {
				0xffffff
			},
		},
	},

	.pio_regions = {
		PIO_RANGE(0x20, 2), /* HACK: PIC */
		PIO_RANGE(0x40, 4), /* PIT */
		PIO_RANGE(0x60, 2), /* HACK: NMI status/control */
		PIO_RANGE(0x64, 1), /* i8042 */
		PIO_RANGE(0x70, 2), /* RTC */
		PIO_RANGE(0x3b0, 0x30), /* VGA */
		PIO_RANGE(0x3e0, 0x918), /* HACK: PCI bus */
		PIO_RANGE(0xd00, 0xf300), /* HACK: PCI bus */
	},

	.pci_devices = {
		/* PCIDevice: 00:00.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x0,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:01.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x8,
			.caps_start = 0,
			.num_caps = 4,
		},
		/* PCIDevice: 00:01.1 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x9,
			.caps_start = 0,
			.num_caps = 4,
		},
		/* PCIDevice: 00:02.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x10,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:02.3 */
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.domain = 0x0,
			.bdf = 0x13,
			.caps_start = 4,
			.num_caps = 5,
		},
		/* PCIDevice: 00:02.4 */
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.domain = 0x0,
			.bdf = 0x14,
			.caps_start = 4,
			.num_caps = 5,
		},
		/* PCIDevice: 00:10.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x80,
			.caps_start = 9,
			.num_caps = 4,
		},
		/* PCIDevice: 00:11.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x88,
			.caps_start = 13,
			.num_caps = 4,
		},
		/* PCIDevice: 00:12.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x90,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:12.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x92,
			.caps_start = 17,
			.num_caps = 2,
		},
		/* PCIDevice: 00:13.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x98,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:13.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x9a,
			.caps_start = 17,
			.num_caps = 2,
		},
		/* PCIDevice: 00:14.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xa0,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:14.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xa2,
			.caps_start = 19,
			.num_caps = 1,
		},
		/* PCIDevice: 00:14.3 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xa3,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:18.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xc0,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:18.1 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xc1,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:18.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xc2,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:18.3 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xc3,
			.caps_start = 20,
			.num_caps = 1,
		},
		/* PCIDevice: 00:18.4 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xc4,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 00:18.5 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0xc5,
			.caps_start = 0,
			.num_caps = 0,
		},
		/* PCIDevice: 01:00.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x100,
			.caps_start = 21,
			.num_caps = 5,
		},
		/* PCIDevice: 01:00.1 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x101,
			.caps_start = 21,
			.num_caps = 5,
		},
		/* PCIDevice: 01:00.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x102,
			.caps_start = 21,
			.num_caps = 5,
		},
		/* PCIDevice: 01:00.3 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x103,
			.caps_start = 21,
			.num_caps = 5,
		},
		/* PCIDevice: 02:00.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x200,
			.caps_start = 21,
			.num_caps = 5,
		},
	},

	.pci_caps = {
		/* PCIDevice: 00:01.0 */
		/* PCIDevice: 00:01.1 */
		{
			.id = PCI_CAP_ID_VNDR,
			.start = 0x48,
			.len = 2,
			.flags = 0,
		},
		{
			.id = PCI_CAP_ID_PM,
			.start = 0x50,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_EXP,
			.start = 0x58,
			.len = 2,
			.flags = 0,
		},
		{
			.id = PCI_CAP_ID_MSI,
			.start = 0xa0,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		/* PCIDevice: 00:02.3 */
		/* PCIDevice: 00:02.4 */
		{
			.id = PCI_CAP_ID_PM,
			.start = 0x50,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_EXP,
			.start = 0x58,
			.len = 2,
			.flags = 0,
		},
		{
			.id = PCI_CAP_ID_MSI,
			.start = 0xa0,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_SSVID,
			.start = 0xb0,
			.len = 2,
			.flags = 0,
		},
		{
			.id = PCI_CAP_ID_HT,
			.start = 0xb8,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:10.0 */
		{
			.id = PCI_CAP_ID_PM,
			.start = 0x50,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_MSI,
			.start = 0x70,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_MSIX,
			.start = 0x90,
			.len = 12,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_EXP,
			.start = 0xa0,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:11.0 */
		{
			.id = PCI_CAP_ID_PM,
			.start = 0x60,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_SATA,
			.start = 0x70,
			.len = 2,
			.flags = 0,
		},
		{
			.id = PCI_CAP_ID_MSI,
			.start = 0x50,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_AF,
			.start = 0xd0,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:12.2 */
		/* PCIDevice: 00:13.2 */
		{
			.id = PCI_CAP_ID_PM,
			.start = 0xc0,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_DBG,
			.start = 0xe4,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:14.2 */
		{
			.id = PCI_CAP_ID_PM,
			.start = 0x50,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		/* PCIDevice: 00:18.3 */
		{
			.id = PCI_CAP_ID_SECDEV,
			.start = 0xf0,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 01:00.0 */
		/* PCIDevice: 01:00.1 */
		/* PCIDevice: 01:00.2 */
		/* PCIDevice: 01:00.3 */
		/* PCIDevice: 02:00.0 */
		{
			.id = PCI_CAP_ID_PM,
			.start = 0x40,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_MSI,
			.start = 0x50,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_EXP,
			.start = 0x70,
			.len = 2,
			.flags = 0,
		},
		{
			.id = PCI_CAP_ID_MSIX,
			.start = 0xb0,
			.len = 12,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_VPD,
			.start = 0xd0,
			.len = 2,
			.flags = 0,
		},
	},
};
