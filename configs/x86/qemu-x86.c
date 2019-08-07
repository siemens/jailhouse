/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for QEMU Q35 VM, 1 GB RAM, 4 cores,
 * 6 MB hypervisor, 74 MB inmates, 1MB shared mem devices
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
 * Guest kernel command line appendix: memmap=82M$0x3a000000
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[16];
	struct jailhouse_irqchip irqchips[1];
	struct jailhouse_pio pio_regions[12];
	struct jailhouse_pci_device pci_devices[9];
	struct jailhouse_pci_capability pci_caps[11];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
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
			.pci_mmconfig_base = 0xb0000000,
			.pci_mmconfig_end_bus = 0xff,
			.x86 = {
				.pm_timer_address = 0x608,
				.vtd_interrupt_limit = 256,
				.iommu_units = {
					{
						.type = JAILHOUSE_IOMMU_INTEL,
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
			.num_pio_regions = ARRAY_SIZE(config.pio_regions),
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
			.size = 0x3a000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* RAM (inmates) */ {
			.phys_start = 0x3a600000,
			.virt_start = 0x3a600000,
			.size = 0x4b00000,
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
			.size =          0x21000,
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

	.pio_regions = {
		PIO_RANGE(0x0, 0x1f), /* floppy DMA controller */
		PIO_RANGE(0x40, 0x4), /* PIT */
		PIO_RANGE(0x60, 0x2), /* HACK: NMI status/control */
		PIO_RANGE(0x64, 0x1), /* i8042 */
		PIO_RANGE(0x70, 0x2), /* rtc */
		PIO_RANGE(0x1ce, 0x3), /* vbe */
		PIO_RANGE(0x2f8, 0x8), /* serial2 */
		PIO_RANGE(0x3b0, 0x8), /* VGA */
		PIO_RANGE(0x3f0, 0x8), /* floppy */
		PIO_RANGE(0x402, 0x1), /* invalid but accessed by X */
		PIO_RANGE(0x5658, 0x4), /* vmport */
		PIO_RANGE(0xc000, 0xff), /* PCI devices */
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
			.id = PCI_CAP_ID_MSI,
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
			.id = PCI_CAP_ID_SATA,
			.start = 0xa8,
			.len = 2,
			.flags = 0,
		},
		{
			.id = PCI_CAP_ID_MSI,
			.start = 0x80,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{ /* virtio-9p-pci */
			.id = PCI_CAP_ID_MSIX,
			.start = 0x98,
			.len = 12,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{ /* e1000e */
			.id = PCI_CAP_ID_PM,
			.start = 0xc8,
			.len = 8,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_MSI,
			.start = 0xd0,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_EXP,
			.start = 0xe0,
			.len = 20,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_CAP_ID_MSIX,
			.start = 0xa0,
			.len = 12,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
		{
			.id = PCI_EXT_CAP_ID_ERR | JAILHOUSE_PCI_EXT_CAP,
			.start = 0x100,
			.len = 4,
			.flags = 0,
		},
		{
			.id = PCI_EXT_CAP_ID_DSN | JAILHOUSE_PCI_EXT_CAP,
			.start = 0x140,
			.len = 4,
			.flags = 0,
		},
	},
};
