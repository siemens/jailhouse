/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Configuration for Gigabyte Technology Co., Ltd. GA-F2A88XM-HD3 Rev. 3.0
 * board with AMD A10-7800 APU and 1G RAM.
 *
 * Created with 'jailhouse config create f2a88xm-hd3.c' and adjusted
 * by Valentine Sinitsyn <valentine.sinitsyn@gmail.com>.
 *
 * NOTE: This config expects the following to be appended to your kernel cmdline
 *       "memmap=0x4200000$0x3b000000"
 */

#include <linux/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[35];
	struct jailhouse_irqchip irqchips[2];
	__u8 pio_bitmap[0x2000];
	struct jailhouse_pci_device pci_devices[26];
	struct jailhouse_pci_capability pci_caps[27];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.hypervisor_memory = {
			.phys_start = 0x3b000000,
			.size = 0x4000000,
		},
		.platform_info.x86 = {
			.mmconfig_base = 0xe0000000,
			.mmconfig_end_bus = 0xff,
			.pm_timer_address = 0x808,
			.iommu_base = {
				0xfeb80000,
			},
		},
		.device_limit = 128,
		.interrupt_limit = 256,
		.root_cell = {
			.name = "F2A88XM-HD3",
			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.pio_bitmap_size = ARRAY_SIZE(config.pio_bitmap),
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
		/* MemRegion: 000c0000-000cf5ff : Video ROM */
		{
			.phys_start = 0xc0000,
			.virt_start = 0xc0000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: 000e0000-000fffff : System ROM */
		{
			.phys_start = 0xe0000,
			.virt_start = 0xe0000,
			.size = 0x20000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* MemRegion: 00100000-00ffffff : System RAM */
		{
			.phys_start = 0x100000,
			.virt_start = 0x100000,
			.size = 0xf00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 01000000-01ffffff : Kernel */
		{
			.phys_start = 0x1000000,
			.virt_start = 0x1000000,
			.size = 0x1000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 02000000-3affffff : System RAM */
		{
			.phys_start = 0x2000000,
			.virt_start = 0x2000000,
			.size = 0x39000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 3f200000-6b8ecfff : System RAM */
		{
			.phys_start = 0x3f200000,
			.virt_start = 0x3f200000,
			.size = 0x2c6ed000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 6b91d000-6bbdffff : System RAM */
		{
			.phys_start = 0x6b91d000,
			.virt_start = 0x6b91d000,
			.size = 0x2c3000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 6bbe0000-6bcadfff : ACPI Non-volatile Storage */
		{
			.phys_start = 0x6bbe0000,
			.virt_start = 0x6bbe0000,
			.size = 0xce000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: 6d1c9000-6d1c9fff : System RAM */
		{
			.phys_start = 0x6d1c9000,
			.virt_start = 0x6d1c9000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 6d1ca000-6d3cffff : ACPI Non-volatile Storage */
		{
			.phys_start = 0x6d1ca000,
			.virt_start = 0x6d1ca000,
			.size = 0x206000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: 6d3d0000-6d850fff : System RAM */
		{
			.phys_start = 0x6d3d0000,
			.virt_start = 0x6d3d0000,
			.size = 0x481000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 6dfe2000-6dffffff : System RAM */
		{
			.phys_start = 0x6dfe2000,
			.virt_start = 0x6dfe2000,
			.size = 0x1e000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* MemRegion: 6e000000-6fffffff : RAM buffer */
		{
			.phys_start = 0x6e000000,
			.virt_start = 0x6e000000,
			.size = 0x2000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
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
		/* MemRegion: d0801000-d0803fff : r8169 */
		{
			.phys_start = 0xd0801000,
			.virt_start = 0xd0801000,
			.size = 0x3000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fea00000-fea00fff : r8169 */
		{
			.phys_start = 0xfea00000,
			.virt_start = 0xfea00000,
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
		/* MemRegion: feb68000-feb68fff : xhci_hcd */
		{
			.phys_start = 0xfeb68000,
			.virt_start = 0xfeb68000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6a000-feb6afff : xhci_hcd */
		{
			.phys_start = 0xfeb6a000,
			.virt_start = 0xfeb6a000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6c000-feb6cfff : ohci_hcd */
		{
			.phys_start = 0xfeb6c000,
			.virt_start = 0xfeb6c000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6d000-feb6d0ff : ehci_hcd */
		{
			.phys_start = 0xfeb6d000,
			.virt_start = 0xfeb6d000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6e000-feb6efff : ohci_hcd */
		{
			.phys_start = 0xfeb6e000,
			.virt_start = 0xfeb6e000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb6f000-feb6f0ff : ehci_hcd */
		{
			.phys_start = 0xfeb6f000,
			.virt_start = 0xfeb6f000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb70000-feb70fff : ohci_hcd */
		{
			.phys_start = 0xfeb70000,
			.virt_start = 0xfeb70000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb71000-feb717ff : ahci */
		{
			.phys_start = 0xfeb71000,
			.virt_start = 0xfeb71000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb80000-febfffff : pnp 00:02 */
		{
			.phys_start = 0xfeb80000,
			.virt_start = 0xfeb80000,
			.size = 0x80000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fed00000-fed003ff : HPET 0 */
		{
			.phys_start = 0xfed00000,
			.virt_start = 0xfed00000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fed61000-fed70fff : pnp 00:09 */
		{
			.phys_start = 0xfed61000,
			.virt_start = 0xfed61000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
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
		/* IOAPIC 0, GSI base 0 */
		{
			.address = 0xfec00000,
			.id = 0x0,
			.pin_bitmap = 0xffffff,
		},
		/* IOAPIC 1, GSI base 24 */
		{
			.address = 0xfec01000,
			.id = 0x0,
			.pin_bitmap = 0xffffff,
		},
	},

	.pio_bitmap = {
		[     0/8 ...   0x1f/8] = -1,
		[  0x20/8 ...   0x27/8] = 0xfc, /* HACK: PIC */
		[  0x28/8 ...   0x3f/8] = -1,
		[  0x40/8 ...   0x47/8] = 0xf0, /* PIT */
		[  0x48/8 ...   0x5f/8] = -1,
		[  0x60/8 ...   0x67/8] = 0xec, /* HACK: NMI status/control */
		[  0x68/8 ...   0x6f/8] = -1,
		[  0x70/8 ...   0x77/8] = 0xfc, /* RTC */
		[  0x78/8 ...   0x7f/8] = -1,
		[  0x80/8 ...   0x87/8] = 0xfe, /* Port 80 (delays) */
		[  0x88/8 ...  0x3af/8] = -1,
		[ 0x3b0/8 ...  0x3df/8] = 0x00, /* VGA */
		[ 0x3e0/8 ...  0xcf7/8] = 0, /* HACK: PCI bus */
		[ 0xcf8/8 ...  0xcff/8] = -1,
		[ 0xd00/8 ... 0xffff/8] = 0, /* HACK: PCI bus */
	},

	.pci_devices = {
		/* PCIDevice: 00:00.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0,
			.bdf = 0x0,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:01.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x8,
			.caps_start = 3,
			.num_caps = 4,
			.num_msi_vectors = 1,
			.msi_64bits = 1,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:01.1 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x9,
			.caps_start = 3,
			.num_caps = 4,
			.num_msi_vectors = 1,
			.msi_64bits = 1,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:02.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x10,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:03.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x18,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:03.1 */
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x19,
			.caps_start = 7,
			.num_caps = 5,
			.num_msi_vectors = 1,
			.msi_64bits = 1,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:04.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x20,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:10.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x80,
			.caps_start = 12,
			.num_caps = 4,
			.num_msi_vectors = 8,
			.msi_64bits = 1,
			.num_msix_vectors = 8,
			.msix_region_size = 0x1000,
			.msix_address = 0xfeb6b000,
		},
		/* PCIDevice: 00:10.1 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x81,
			.caps_start = 12,
			.num_caps = 4,
			.num_msi_vectors = 8,
			.msi_64bits = 1,
			.num_msix_vectors = 8,
			.msix_region_size = 0x1000,
			.msix_address = 0xfeb69000,
		},
		/* PCIDevice: 00:11.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x88,
			.caps_start = 16,
			.num_caps = 2,
			.num_msi_vectors = 8,
			.msi_64bits = 1,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:12.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x90,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:12.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x92,
			.caps_start = 18,
			.num_caps = 2,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:13.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x98,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:13.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x9a,
			.caps_start = 18,
			.num_caps = 2,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:14.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xa0,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:14.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xa2,
			.caps_start = 20,
			.num_caps = 1,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:14.3 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xa3,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:14.4 */
		{
			.type = JAILHOUSE_PCI_TYPE_BRIDGE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xa4,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:14.5 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xa5,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:18.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xc0,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:18.1 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xc1,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:18.2 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xc2,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:18.3 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xc3,
			.caps_start = 21,
			.num_caps = 1,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:18.4 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xc4,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 00:18.5 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0xc5,
			.caps_start = 0,
			.num_caps = 0,
			.num_msi_vectors = 0,
			.msi_64bits = 0,
			.num_msix_vectors = 0,
			.msix_region_size = 0x0,
			.msix_address = 0x0,
		},
		/* PCIDevice: 01:00.0 */
		{
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.iommu = 0,
			.domain = 0x0,
			.bdf = 0x100,
			.caps_start = 22,
			.num_caps = 5,
			.num_msi_vectors = 1,
			.msi_64bits = 1,
			.num_msix_vectors = 4,
			.msix_region_size = 0x1000,
			.msix_address = 0xd0800000,
		},
	},

	.pci_caps = {
		/* PCIDevice: 00:00.2 */
		{
			.id = 0xf,
			.start = 0x40,
			.len = 2,
			.flags = 0,
		},
		{
			.id = 0x5,
			.start = 0x54,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x8,
			.start = 0x64,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:01.0 */
		/* PCIDevice: 00:01.1 */
		{
			.id = 0x9,
			.start = 0x48,
			.len = 2,
			.flags = 0,
		},
		{
			.id = 0x1,
			.start = 0x50,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x10,
			.start = 0x58,
			.len = 2,
			.flags = 0,
		},
		{
			.id = 0x5,
			.start = 0xa0,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		/* PCIDevice: 00:03.1 */
		{
			.id = 0x1,
			.start = 0x50,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x10,
			.start = 0x58,
			.len = 2,
			.flags = 0,
		},
		{
			.id = 0x5,
			.start = 0xa0,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0xd,
			.start = 0xb0,
			.len = 2,
			.flags = 0,
		},
		{
			.id = 0x8,
			.start = 0xb8,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:10.0 */
		/* PCIDevice: 00:10.1 */
		{
			.id = 0x1,
			.start = 0x50,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x5,
			.start = 0x70,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x11,
			.start = 0x90,
			.len = 12,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x10,
			.start = 0xa0,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:11.0 */
		{
			.id = 0x5,
			.start = 0x50,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x12,
			.start = 0x70,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:12.2 */
		/* PCIDevice: 00:13.2 */
		{
			.id = 0x1,
			.start = 0xc0,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0xa,
			.start = 0xe4,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 00:14.2 */
		{
			.id = 0x1,
			.start = 0x50,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		/* PCIDevice: 00:18.3 */
		{
			.id = 0xf,
			.start = 0xf0,
			.len = 2,
			.flags = 0,
		},
		/* PCIDevice: 01:00.0 */
		{
			.id = 0x1,
			.start = 0x40,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x5,
			.start = 0x50,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x10,
			.start = 0x70,
			.len = 2,
			.flags = 0,
		},
		{
			.id = 0x11,
			.start = 0xb0,
			.len = 12,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x3,
			.start = 0xd0,
			.len = 2,
			.flags = 0,
		},
	},
};
