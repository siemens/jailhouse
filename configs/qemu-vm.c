/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for QEMU Q35 VM, 1 GB RAM, 64 MB hypervisor (-192K ACPI)
 * Command line:
 * qemu-system-x86_64 -machine q35 -m 1G -enable-kvm -smp 4 \
 *  -drive file=/path/to/image,id=disk,if=none -device ide-hd,drive=disk \
 *  -virtfs local,path=/local/path,security_model=passthrough,mount_tag=host \
 *  -cpu kvm64,-kvm_pv_eoi,-kvm_steal_time,-kvm_asyncpf,-kvmclock,+vmx,+x2apic
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

#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[5];
	struct jailhouse_irqchip irqchips[1];
	__u8 pio_bitmap[0x2000];
	struct jailhouse_pci_device pci_devices[6];
	struct jailhouse_pci_capability pci_caps[3];
} __attribute__((packed)) config = {
	.header = {
		.hypervisor_memory = {
			.phys_start = 0x3c000000,
			.size = 0x4000000 - 0x30000,
		},
		.config_memory = {
			.phys_start = 0x3ffdf000,
			.size = 0x30000,
		},
		.platform_info.x86 = {
			.pm_timer_address = 0x608,
		},
		.root_cell = {
			.name = "QEMU Linux VM",

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
			.size = 0x3c000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		/* ACPI */ {
			.phys_start = 0x3ffdf000,
			.virt_start = 0x3ffdf000,
			.size = 0x30000,
			.flags = JAILHOUSE_MEM_READ,
		},
		/* PCI */ {
			.phys_start = 0xc0000000,
			.virt_start = 0xc0000000,
			.size = 0x3ec00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* not safe until we catch MSIs via interrupt remapping */
		/* HPET */ {
			.phys_start = 0xfed00000,
			.virt_start = 0xfed00000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
	},

	.irqchips = {
		/* IOAPIC */ {
			.address = 0xfec00000,
			.pin_bitmap = 0xffffff,
		},
	},

	.pio_bitmap = {
		[     0/8 ...   0x3f/8] = -1,
		[  0x40/8 ...   0x47/8] = 0xf0, /* PIT */
		[  0x48/8 ...   0x5f/8] = -1,
		[  0x60/8 ...   0x67/8] = 0xec, /* HACK: 8042, PC speaker - and more */
		[  0x68/8 ...   0x6f/8] = -1,
		[  0x70/8 ...   0x77/8] = 0xfc, /* rtc */
		[  0x78/8 ...   0x7f/8] = -1,
		[  0x80/8 ...   0x87/8] = 0xfe, /* port 80 (delays) */
		[  0x88/8 ...  0x2f7/8] = -1,
		[ 0x2f8/8 ...  0x2ff/8] = 0, /* serial2 */
		[ 0x300/8 ...  0x3af/8] = -1,
		[ 0x3b0/8 ...  0x3df/8] = 0, /* VGA */
		[ 0x3e0/8 ... 0x5657/8] = -1,
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
		{ /* e1000 */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x0010,
		},
		{ /* virtio-9p-pci */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x0018,
			.caps_start = 0,
			.num_caps = 1,
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
		},
		{ /* SMBus */
			.type = JAILHOUSE_PCI_TYPE_DEVICE,
			.domain = 0x0000,
			.bdf = 0x00fb,
		},
	},

	.pci_caps = {
		{ /* virtio-9p-pci */
			.id = 0x11,
			.start = 0x40,
			.len = 12,
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
			.start = 0x50,
			.len = 14,
			.flags = JAILHOUSE_PCICAPS_WRITE,
		},
	},
};
