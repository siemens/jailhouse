/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

struct boot_params {
	u8	padding1[0x230];
	u32	kernel_alignment;
	u8	padding2[0x250 - 0x230 - 4];
	u64	setup_data;
	u8	padding3[8];
	u32	init_size;
} __attribute__((packed));

struct setup_data {
	u64	next;
	u32	type;
	u32	length;
	u16	version;
	u16	compatible_version;
	u16	pm_timer_address;
	u16	num_cpus;
	u64	pci_mmconfig_base;
	u32	tsc_khz;
	u32	apic_khz;
	u8	standard_ioapic;
	u8	cpu_ids[SMP_MAX_CPUS];
	/* Flags bits 0-3: has access to platform UART n */
	u32	flags;
} __attribute__((packed));

/* We use the cmdline section for zero page and setup data. */
static union {
	struct boot_params params;
	char __reservation[PAGE_SIZE * 3];
} boot __attribute__((section(".cmdline")));

void inmate_main(void)
{
	void (*entry)(int, struct boot_params *);
	struct setup_data *setup_data;
	void *kernel;

	kernel = (void *)(unsigned long)boot.params.kernel_alignment;

	map_range(kernel, boot.params.init_size, MAP_CACHED);

	setup_data = (struct setup_data *)boot.params.setup_data;
	setup_data->pm_timer_address = comm_region->pm_timer_address;
	setup_data->pci_mmconfig_base = comm_region->pci_mmconfig_base;
	setup_data->tsc_khz = comm_region->tsc_khz;
	setup_data->apic_khz = comm_region->apic_khz;
	setup_data->num_cpus = comm_region->num_cpus;

	smp_wait_for_all_cpus();
	memcpy(setup_data->cpu_ids, smp_cpu_ids, SMP_MAX_CPUS);

	entry = kernel + 0x200;
	entry(0, &boot.params);
}
