/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
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
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/control.h>
#include <jailhouse/string.h>
#include <generated/version.h>
#include <asm/spinlock.h>

extern u8 __text_start[], __page_pool[];

static const __attribute__((aligned(PAGE_SIZE))) u8 empty_page[PAGE_SIZE];

static DEFINE_SPINLOCK(init_lock);
static unsigned int master_cpu_id = -1;
static volatile unsigned int initialized_cpus;
static volatile int error;

static void init_early(unsigned int cpu_id)
{
	unsigned long core_and_percpu_size = hypervisor_header.core_size +
		sizeof(struct per_cpu) * hypervisor_header.max_cpus;
	unsigned long hyp_phys_start, hyp_phys_end;
	struct jailhouse_memory hv_page;

	master_cpu_id = cpu_id;

	system_config = (struct jailhouse_system *)
		(JAILHOUSE_BASE + core_and_percpu_size);

	arch_dbg_write_init();

	printk("\nInitializing Jailhouse hypervisor %s on CPU %d\n",
	       JAILHOUSE_VERSION, cpu_id);
	printk("Code location: %p\n", __text_start);

	error = paging_init();
	if (error)
		return;

	root_cell.config = &system_config->root_cell;

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
	hyp_phys_start = system_config->hypervisor_memory.phys_start;
	hyp_phys_end = hyp_phys_start + system_config->hypervisor_memory.size;

	hv_page.phys_start = paging_hvirt2phys(empty_page);
	hv_page.virt_start = hyp_phys_start;
	hv_page.size = PAGE_SIZE;
	hv_page.flags = JAILHOUSE_MEM_READ;
	while (hv_page.virt_start < hyp_phys_end) {
		error = arch_map_memory_region(&root_cell, &hv_page);
		if (error)
			return;
		hv_page.virt_start += PAGE_SIZE;
	}

	paging_dump_stats("after early setup");
	printk("Initializing processors:\n");
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

	/*
	 * If this CPU is last, make sure everything was committed before we
	 * signal the other CPUs spinning on initialized_cpus that they can
	 * continue.
	 */
	memory_barrier();
	initialized_cpus++;
	return;

failed:
	printk("FAILED\n");
	error = err;
}

int map_root_memory_regions(void)
{
	const struct jailhouse_memory *mem;
	unsigned int n;
	int err;

	for_each_mem_region(mem, root_cell.config, n) {
		if (JAILHOUSE_MEMORY_IS_SUBPAGE(mem))
			err = mmio_subpage_register(&root_cell, mem);
		else
			err = arch_map_memory_region(&root_cell, mem);
		if (err)
			return err;
	}
	return 0;
}

static void init_late(void)
{
	unsigned int cpu, expected_cpus = 0;

	for_each_cpu(cpu, root_cell.cpu_set)
		expected_cpus++;
	if (hypervisor_header.online_cpus != expected_cpus) {
		error = -EINVAL;
		return;
	}

	error = arch_init_late();
	if (error)
		return;

	config_commit(&root_cell);

	paging_dump_stats("after late setup");
}

int entry(unsigned int cpu_id, struct per_cpu *cpu_data)
{
	static volatile bool activate;
	bool master = false;

	cpu_data->cpu_id = cpu_id;

	spin_lock(&init_lock);

	if (master_cpu_id == -1) {
		master = true;
		init_early(cpu_id);
	}

	if (!error)
		cpu_init(cpu_data);

	spin_unlock(&init_lock);

	while (!error && initialized_cpus < hypervisor_header.online_cpus)
		cpu_relax();

	if (!error && master) {
		init_late();
		if (!error) {
			/*
			 * Make sure everything was committed before we signal
			 * the other CPUs that they can continue.
			 */
			memory_barrier();
			activate = true;
		}
	} else {
		while (!error && !activate)
			cpu_relax();
	}

	if (error) {
		if (master)
			arch_shutdown();
		arch_cpu_restore(cpu_data, error);
		return error;
	}

	if (master)
		printk("Activating hypervisor\n");

	/* point of no return */
	arch_cpu_activate_vmm(cpu_data);
}

/** Hypervisor description header. */
struct jailhouse_header __attribute__((section(".header")))
hypervisor_header = {
	.signature = JAILHOUSE_SIGNATURE,
	.core_size = (unsigned long)__page_pool - JAILHOUSE_BASE,
	.percpu_size = sizeof(struct per_cpu),
	.entry = arch_entry - JAILHOUSE_BASE,
};
