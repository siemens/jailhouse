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

#define ZERO_PAGE_ADDR		0xf5000UL

struct boot_params {
	u8	padding1[0x230];
	u32	kernel_alignment;
	u8	padding2[0x250 - 0x230 - 4];
	u64	setup_data;
	u8	padding3[8];
	u32	init_size;
};

struct setup_data {
	u64	next;
	u32	type;
	u32	length;
	u16	pm_timer_address;
	u16	num_cpus;
	u8	cpu_ids[SMP_MAX_CPUS];
};

void inmate_main(void)
{
	struct boot_params *boot_params = (struct boot_params *)ZERO_PAGE_ADDR;
	void (*entry)(int, struct boot_params *);
	struct setup_data *setup_data;
	void *kernel;

	kernel = (void *)(unsigned long)boot_params->kernel_alignment;

	map_range(kernel, boot_params->init_size, MAP_CACHED);

	setup_data = (struct setup_data *)boot_params->setup_data;
	setup_data->pm_timer_address = comm_region->pm_timer_address;
	setup_data->num_cpus = comm_region->num_cpus;

	smp_wait_for_all_cpus();
	memcpy(setup_data->cpu_ids, smp_cpu_ids, SMP_MAX_CPUS);

	entry = kernel + 0x200;
	entry(0, boot_params);
}
