/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Test configuration for QEMU VM, 1 GB RAM, 64 MB hypervisor (-8 K ACPI)
 * Command line:
 * qemu-system-x86_64 /path/to/image -m 1G -enable-kvm -smp 4 \
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

#define ALIGN __attribute__((aligned(1)))
#define ARRAY_SIZE(a) sizeof(a) / sizeof(a[0])

struct {
	struct jailhouse_system ALIGN header;
	__u64 ALIGN cpus[1];
	struct jailhouse_memory ALIGN mem_regions[5];
	__u8 ALIGN pio_bitmap[0x2000];
} ALIGN config = {
	.header = {
		.hypervisor_memory = {
			.phys_start = 0x3c000000,
			.size = 0x4000000 - 0x2000,
		},
		.config_memory = {
			.phys_start = 0x3fffe000,
			.size = 0x2000,
		},
		.system = {
			.name = "QEMU Linux VM",

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irq_lines = 0,
			.pio_bitmap_size = ARRAY_SIZE(config.pio_bitmap),

			.num_pci_devices = 0,
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
		/* ACPI */ {
			.phys_start = 0x3fffe000,
			.virt_start = 0x3fffe000,
			.size = 0x2000,
			.access_flags = JAILHOUSE_MEM_READ,
		},
		/* PCI */ {
			.phys_start = 0x80000000,
			.virt_start = 0x80000000,
			.size = 0x7ec00000,
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
	},

	.pio_bitmap = {
		[     0/8 ...   0x1f/8] = -1,
		[  0x20/8 ...   0x27/8] = 0xfc, /* HACK: PIC */
		[  0x28/8 ...   0x3f/8] = -1,
		[  0x40/8 ...   0x47/8] = 0xf0, /* PIT */
		[  0x48/8 ...   0x5f/8] = -1,
		[  0x60/8 ...   0x67/8] = 0xec, /* HACK: 8042, PC speaker - and more */
		[  0x68/8 ...   0x6f/8] = -1,
		[  0x70/8 ...   0x77/8] = 0xfc, /* rtc */
		[  0x78/8 ...   0x7f/8] = -1,
		[  0x80/8 ...   0x87/8] = 0xfe, /* port 80 (delays) */
		[  0x88/8 ...  0x16f/8] = -1,
		[ 0x170/8 ...  0x177/8] = 0, /* ide */
		[ 0x178/8 ...  0x1ef/8] = -1,
		[ 0x1f0/8 ...  0x1f7/8] = 0, /* ide */
		[ 0x1f8/8 ...  0x2f7/8] = -1,
		[ 0x2f8/8 ...  0x2ff/8] = 0, /* serial2 */
		[ 0x300/8 ...  0x36f/8] = -1,
		[ 0x370/8 ...  0x377/8] = 0xbf, /* ide */
		[ 0x378/8 ...  0x3af/8] = -1,
		[ 0x3b0/8 ...  0x3df/8] = 0, /* VGA */
		[ 0x3e0/8 ...  0x3ef/8] = -1,
		[ 0x3f0/8 ...  0x3f7/8] = 0xbf, /* ide */
		[ 0x3f8/8 ...  0xcf7/8] = -1,
		[ 0xcf8/8 ...  0xcff/8] = 0, /* HACK: PCI, PIIX3 RCR */
		[ 0xd00/8 ... 0x5657/8] = -1,
		[0x5658/8 ... 0x565f/8] = 0xf0, /* vmport */
		[0x5660/8 ... 0xc03f/8] = -1,
		[0xc000/8 ... 0xc03f/8] = 0, /* virtio-9p-pci */
		[0xc040/8 ... 0xc07f/8] = 0, /* e1000 */
		[0xc080/8 ... 0xc08f/8] = 0, /* piix bmdma */
		[0xc090/8 ... 0xffff/8] = -1,
	},
};
