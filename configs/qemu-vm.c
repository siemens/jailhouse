/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for QEMU Q35 VM, 1 GB RAM, 4 cores,
 * 6 MB hypervisor, 60 MB inmates (-4K shared mem device)
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * See README.md for QEMU command lines on Intel and AMD.
 * Guest kernel command line appendix: memmap=66M$0x3b000000
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[16];
	struct jailhouse_irqchip irqchips[1];
	__u8 pio_bitmap[0x2000];
	struct jailhouse_pci_device pci_devices[9];
	struct jailhouse_pci_capability pci_caps[11];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.hypervisor_memory = {
			.phys_start = 0x3b000000,
			.size = 0x600000,
		},
		.debug_console = {
			.address = 0x3f8,
			/* .divider = 0x1, */
			.flags = JAILHOUSE_CON1_TYPE_8250 |
				 JAILHOUSE_CON1_ACCESS_PIO |
				 JAILHOUSE_CON2_TYPE_ROOTPAGE,
		},
		.platform_info = {
			.pci_mmconfig_base = 0xb0000000,
			.pci_mmconfig_end_bus = 0xff,
			.x86 = {
				.pm_timer_address = 0x608,
				.vtd_interrupt_limit = 256,
				.iommu_units = {
					{
						.base = 0xfed90000,
						.size = 0x1000,
					},
				},
			},
		},
		.root_cell = {
			.name = "QEMU-VM",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.pio_bitmap_size = ARRAY_SIZE(config.pio_bitmap),
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),
			.num_pci_caps = ARRAY_SIZE(config.pci_caps),
		},
	},

	.cpus = {
		0xf,
	},

	.mem_regions = {
		/* RAM */ {
			.phys_start = 0x0,
			.virt_start = 0x0,
			.size = 0x3b000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* RAM (inmates) */ {
			.phys_start = 0x3b600000,
			.virt_start = 0x3b600000,
			.size = 0x3b00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* RAM */ {
			.phys_start = 0x3f200000,
			.virt_start = 0x3f200000,
			.size = 0xddf000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* ACPI */ {
			.phys_start = 0x3ffdf000,
			.virt_start = 0x3ffdf000,
			.size = 0x30000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* MemRegion: fd000000-fdffffff : 0000:00:01.0 (vesafb) */
		{
			.phys_start = 0xfd000000,
			.virt_start = 0xfd000000,
			.size = 0x1000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fe000000-fe003fff : 0000:00:1f.7 (virtio-9p) */
		{
			.phys_start = 0xfe000000,
			.virt_start = 0xfe000000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb40000-feb7ffff : 0000:00:02.0 */
		{
			.phys_start = 0xfeb40000,
			.virt_start = 0xfeb40000,
			.size = 0x40000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feb80000-feb9ffff : e1000e */
		{
			.phys_start = 0xfeb80000,
			.virt_start = 0xfeb80000,
			.size = 0x20000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: feba0000-febbffff : e1000e */
		{
			.phys_start = 0xfeba0000,
			.virt_start = 0xfeba0000,
			.size = 0x20000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: febd1000-febd3fff : e1000e */
		{
			.phys_start = 0xfebd1000,
			.virt_start = 0xfebd1000,
			.size = 0x3000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: febd4000-febd7fff : 0000:00:1b.0 (ICH HD audio) */
		{
			.phys_start = 0xfebd4000,
			.virt_start = 0xfebd4000,
			.size = 0x4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: febd8000-febd8fff : 0000:00:01.0 (vesafd) */
		{
			.phys_start = 0xfebd8000,
			.virt_start = 0xfebd8000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: febd9000-febd9fff : 0000:00:1f.2 (ahci) */
		{
			.phys_start = 0xfebd9000,
			.virt_start = 0xfebd9000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* MemRegion: fed00000-fed003ff : PNP0103:00 (HPET) */
		{
			.phys_start = 0xfed00000,
			.virt_start = 0xfed00000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* IVSHMEM shared memory region (networking) */
		{
			.phys_start = 0x3f100000,
			.virt_start = 0x3f100000,
			.size = 0xff000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* IVSHMEM shared memory region (demo) */
		{
			.phys_start = 0x3f1ff000,
			.virt_start = 0x3f1ff000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
	},

	.irqchips = {
		/* IOAPIC */ {
			.address = 0xfec00000,
			.id = 0xff00,
			.pin_bitmap = {
				    0xffffff
			},
		},
	},

	.pio_bitmap = {
		[     0/8 ...   0x1f/8] = 0, /* floppy DMA controller */
		[  0x20/8 ...   0x3f/8] = -1,
		[  0x40/8 ...   0x47/8] = 0xf0, /* PIT */
		[  0x48/8 ...   0x5f/8] = -1,
		[  0x60/8 ...   0x67/8] = 0xec, /* HACK: NMI status/control */
		[  0x68/8 ...   0x6f/8] = -1,
		[  0x70/8 ...   0x77/8] = 0xfc, /* rtc */
		[  0x78/8 ...   0x7f/8] = -1,
		[  0x80/8 ...   0x87/8] = 0xfe, /* port 80 (delays) */
		[  0x88/8 ...  0x1c7/8] = -1,
		[ 0x1c8/8 ...  0x1cf/8] = 0x3f, /* vbe */
		[ 0x1d0/8 ...  0x1d7/8] = 0xfe, /* vbe */
		[ 0x1d8/8 ...  0x2f7/8] = -1,
		[ 0x2f8/8 ...  0x2ff/8] = 0, /* serial2 */
		[ 0x300/8 ...  0x3af/8] = -1,
		[ 0x3b0/8 ...  0x3df/8] = 0, /* VGA */
		[ 0x3e0/8 ...  0x3ef/8] = -1,
		[ 0x3f0/8 ...  0x3f7/8] = 0, /* floppy */
		[ 0x3f8/8 ...  0x3ff/8] = -1,
		[ 0x400/8 ...  0x407/8] = 0xfb, /* invalid but accessed by X */
		[ 0x408/8 ... 0x5657/8] = -1,
		[0x5658/8 ... 0x565f/8] = 0xf0, /* vmport */
		[0x5660/8 ... 0xbfff/8] = -1,
		[0xc000/8 ... 0xc0ff/8] = 0, /* PCI devices */
		[0xc100/8 ... 0xffff/8] = -1,
	},

	.pci_devices = {
		{ /* VGA */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x0008,
		},
		{ /* e1000e */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x0010,
			.bar_mask = {
				0xfffe0000, 0xfffe0000, 0xffffffe0,
				0xffffc000, 0x00000000, 0x00000000,
			},
			.caps_start = 5,
			.num_caps = 6,
			.num_msi_vectors = 1,
			.msi_64bits = 1,
			.num_msix_vectors = 5,
			.msix_region_size = 0x1000,
			.msix_address = 0xfebd0000,
		},
		{ /* ICH HD audio */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x00d8,
			.caps_start = 0,
			.num_caps = 2,
			.num_msi_vectors = 1,
			.msi_64bits = 1,
		},
		{ /* ISA bridge */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x00f8,
		},
		{ /* AHCI */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x00fa,
			.caps_start = 2,
			.num_caps = 2,
			.num_msi_vectors = 1,
			.msi_64bits = 1,
		},
		{ /* SMBus */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x00fb,
		},
		{ /* virtio-9p-pci */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x00ff,
			.bar_mask = {
				0xffffffe0, 0xfffff000, 0x00000000,
				0x00000000, 0xffffc000, 0xffffffff,
			},
			.caps_start = 4,
			.num_caps = 1,
			.num_msix_vectors = 2,
			.msix_region_size = 0x1000,
			.msix_address = 0xfebda000,
		},
		{ /* IVSHMEM (networking) */
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 0x0000,
			.bdf = 0x0e << 3,
			.bar_mask = {
				0xffffff00, 0xffffffff, 0x00000000,
				0x00000000, 0xffffffe0, 0xffffffff,
			},
			.num_msix_vectors = 1,
			.shmem_region = 14,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
		{ /* IVSHMEM (demo) */
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 0x0000,
			.bdf = 0x0f << 3,
			.bar_mask = {
				0xffffff00, 0xffffffff, 0x00000000,
				0x00000000, 0xffffffe0, 0xffffffff,
			},
			.num_msix_vectors = 1,
			.shmem_region = 15,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_UNDEFINED,
		},
	},

	.pci_caps = {
		{ /* ICH HD audio */
			.id = 0x5,
			.start = 0x60,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{ /* non-cap registers: HDCTL, TCSEL, DCKCTL, DCKSTS */
			.start = 0x40,
			.len = 0x10,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{ /* AHCI */
			.id = 0x12,
			.start = 0xa8,
			.len = 2,
			.flags = 0,
		},
		{
			.id = 0x5,
			.start = 0x80,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{ /* virtio-9p-pci */
			.id = 0x11,
			.start = 0x98,
			.len = 12,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{ /* e1000e */
			.id = 0x1,
			.start = 0xc8,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x5,
			.start = 0xd0,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x10,
			.start = 0xe0,
			.len = 20,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x11,
			.start = 0xa0,
			.len = 12,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = 0x1 | JAILHOUSE_PCI_EXT_CAP,
			.start = 0x100,
			.len = 4,
			.flags = 0,
		},
		{
			.id = 0x3 | JAILHOUSE_PCI_EXT_CAP,
			.start = 0x140,
			.len = 4,
			.flags = 0,
		},
	},
};
