/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/processor.h>
#include <jailhouse/printk.h>
#include <jailhouse/entry.h>
#include <jailhouse/paging.h>
#include <jailhouse/control.h>
#include <jailhouse/string.h>
#include <asm/spinlock.h>

extern u8 __text_start[], __hv_core_end[];

static const __attribute__((aligned(PAGE_SIZE))) u8 empty_page[PAGE_SIZE];

struct cell root_cell;

static DEFINE_SPINLOCK(init_lock);
static unsigned int master_cpu_id = -1;
static volatile unsigned int initialized_cpus;
static volatile int error;

static void init_early(unsigned int cpu_id)
{
	struct jailhouse_memory hv_page;
	unsigned long core_percpu_size;

	master_cpu_id = cpu_id;

	arch_dbg_write_init();

	printk("\nInitializing Jailhouse hypervisor on CPU %d\n", cpu_id);
	printk("Code location: %p\n", __text_start);

	error = paging_init();
	if (error)
		return;

	root_cell.config = &system_config->root_cell;

	error = check_mem_regions(&system_config->root_cell);
	if (error)
		return;

	root_cell.id = -1;
	error = cell_init(&root_cell);
	if (error)
		return;

	error = arch_init_early();
	if (error)
		return;

	/*
	 * Back the region of the hypervisor core and per-CPU page with empty
	 * pages for Linux. This allows to fault-in the hypervisor region into
	 * Linux' page table before shutdown without triggering violations.
	 */
	hv_page.phys_start = page_map_hvirt2phys(empty_page);
	hv_page.virt_start = page_map_hvirt2phys(&hypervisor_header);
	hv_page.size = PAGE_SIZE;
	hv_page.flags = JAILHOUSE_MEM_READ;
	core_percpu_size = PAGE_ALIGN(hypervisor_header.core_size) +
		hypervisor_header.possible_cpus * sizeof(struct per_cpu);
	while (core_percpu_size > 0) {
		error = arch_map_memory_region(&root_cell, &hv_page);
		if (error)
			return;
		core_percpu_size -= PAGE_SIZE;
		hv_page.virt_start += PAGE_SIZE;
	}

	page_map_dump_stats("after early setup");
	printk("Initializing first processor:\n");
}

static void cpu_init(struct per_cpu *cpu_data)
{
	int err = -EINVAL;

	printk(" CPU %d... ", cpu_data->cpu_id);

	if (!cpu_id_valid(cpu_data->cpu_id))
		goto failed;

	cpu_data->cell = &root_cell;

	err = arch_cpu_init(cpu_data);
	if (err)
		goto failed;

	printk("OK\n");

	/* If this CPU is last, make sure everything was committed before we
	 * signal the other CPUs spinning on initialized_cpus that they can
	 * continue. */
	memory_barrier();
	initialized_cpus++;
	return;

failed:
	printk("FAILED\n");
	if (!error)
		error = err;
}

static void init_late(struct per_cpu *cpu_data)
{
	const struct jailhouse_memory *mem =
		jailhouse_cell_mem_regions(root_cell.config);
	unsigned int expected_cpus = 0;
	unsigned int n;

	for_each_cpu(n, root_cell.cpu_set)
		expected_cpus++;
	if (hypervisor_header.online_cpus != expected_cpus) {
		error = -EINVAL;
		return;
	}

	error = arch_init_late();
	if (error)
		return;

	for (n = 0; n < root_cell.config->num_memory_regions; n++, mem++) {
		error = arch_map_memory_region(&root_cell, mem);
		if (error)
			return;
	}

	arch_config_commit(cpu_data, &root_cell);

	page_map_dump_stats("after late setup");
	printk("Initializing remaining processors:\n");
}

int entry(unsigned int cpu_id, struct per_cpu *cpu_data)
{
	bool master = false;

	cpu_data->cpu_id = cpu_id;

	spin_lock(&init_lock);

	if (master_cpu_id == -1) {
		master = true;
		init_early(cpu_id);
	}

	if (!error) {
		cpu_init(cpu_data);

		if (master && !error)
			init_late(cpu_data);
	}

	spin_unlock(&init_lock);

	while (!error && initialized_cpus < hypervisor_header.online_cpus)
		cpu_relax();

	if (error) {
		if (master)
			arch_shutdown();
		arch_cpu_restore(cpu_data);
		return error;
	}

	if (master)
		printk("Activating hypervisor\n");

	/* point of no return */
	arch_cpu_activate_vmm(cpu_data);
}

struct jailhouse_header __attribute__((section(".header")))
hypervisor_header = {
	.signature = JAILHOUSE_SIGNATURE,
	.core_size = (unsigned long)__hv_core_end - JAILHOUSE_BASE,
	.percpu_size = sizeof(struct per_cpu),
	.entry = arch_entry,
};
