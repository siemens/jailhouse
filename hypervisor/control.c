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

#include <jailhouse/entry.h>
#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/paging.h>
#include <jailhouse/string.h>
#include <asm/bitops.h>
#include <asm/spinlock.h>

struct jailhouse_system *system_config;
struct cell *cell_list;

static DEFINE_SPINLOCK(shutdown_lock);

unsigned int next_cpu(unsigned int cpu, struct cpu_set *cpu_set, int exception)
{
	do
		cpu++;
	while (cpu <= cpu_set->max_cpu_id &&
	       (cpu == exception || !test_bit(cpu, cpu_set->bitmap)));
	return cpu;
}

static void cell_suspend(struct per_cpu *cpu_data)
{
	struct cell *cell = cpu_data->cell;
	unsigned int cpu;

	for_each_cpu_except(cpu, cell->cpu_set, cpu_data->cpu_id)
		arch_suspend_cpu(cpu);
	printk("Suspended cell \"%s\"\n", cell->name);
}

static void cell_resume(struct per_cpu *cpu_data)
{
	unsigned int cpu;

	for_each_cpu_except(cpu, cpu_data->cell->cpu_set, cpu_data->cpu_id)
		arch_resume_cpu(cpu);
}

static unsigned int get_free_cell_id(void)
{
	unsigned int id = 0;
	struct cell *cell;

retry:
	for (cell = cell_list; cell; cell = cell->next)
		if (cell->id == id) {
			id++;
			goto retry;
		}

	return id;
}

int cell_init(struct cell *cell, struct jailhouse_cell_desc *config,
	      bool copy_cpu_set)
{
	unsigned long *config_cpu_set =
		(unsigned long *)(((void *)config) +
				  sizeof(struct jailhouse_cell_desc));
	unsigned long cpu_set_size = config->cpu_set_size;
	struct jailhouse_memory *config_ram =
		(struct jailhouse_memory *)(((void *)config_cpu_set) +
					    cpu_set_size);
	struct cpu_set *cpu_set;

	memcpy(cell->name, config->name, sizeof(cell->name));
	cell->id = get_free_cell_id();

	if (cpu_set_size > PAGE_SIZE)
		return -EINVAL;
	else if (cpu_set_size > sizeof(cell->small_cpu_set.bitmap)) {
		cpu_set = page_alloc(&mem_pool, 1);
		if (!cpu_set)
			return -ENOMEM;
		cpu_set->max_cpu_id =
			((PAGE_SIZE - sizeof(unsigned long)) * 8) - 1;
	} else {
		cpu_set = &cell->small_cpu_set;
		cpu_set->max_cpu_id =
			(sizeof(cell->small_cpu_set.bitmap) * 8) - 1;
	}
	cell->cpu_set = cpu_set;
	if (copy_cpu_set)
		memcpy(cell->cpu_set->bitmap, config_cpu_set, cpu_set_size);

	cell->page_offset = config_ram->phys_start;

	return 0;
}

static void destroy_cpu_set(struct cell *cell)
{
	if (cell->cpu_set != &cell->small_cpu_set)
		page_free(&mem_pool, cell->cpu_set, 1);
}

int check_mem_regions(struct jailhouse_cell_desc *config)
{
	struct jailhouse_memory *mem;
	unsigned int n;

	mem = (void *)config + sizeof(struct jailhouse_cell_desc) +
		config->cpu_set_size;

	for (n = 0; n < config->num_memory_regions; n++, mem++) {
		if (mem->phys_start & ~PAGE_MASK ||
		    mem->virt_start & ~PAGE_MASK ||
		    mem->size & ~PAGE_MASK ||
		    mem->access_flags & ~JAILHOUSE_MEM_VALID_FLAGS) {
			printk("FATAL: Invalid memory bar (%p, %p, %p, %x)\n",
			       mem->phys_start, mem->virt_start, mem->size,
			       mem->access_flags);
			return -EINVAL;
		}
	}
	return 0;
}

int cell_create(struct per_cpu *cpu_data, unsigned long config_address)
{
	unsigned long header_size, total_size;
	struct jailhouse_cell_desc *cfg;
	struct cpu_set *shrinking_set;
	unsigned int cell_pages, cpu;
	struct cell *cell, *last;
	int err;

	cell_suspend(cpu_data);

	header_size = (config_address & ~PAGE_MASK) +
		sizeof(struct jailhouse_cell_desc);

	err = page_map_create(hv_page_table, config_address & PAGE_MASK,
			      header_size, FOREIGN_MAPPING_BASE,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS);
	if (err)
		goto resume_out;

	cfg = (struct jailhouse_cell_desc *)(FOREIGN_MAPPING_BASE +
					     (config_address & ~PAGE_MASK));
	total_size = jailhouse_cell_config_size(cfg);
	if (total_size >
	    hypervisor_header.possible_cpus * NUM_FOREIGN_PAGES * PAGE_SIZE) {
		total_size = PAGE_SIZE;
		err = -ENOMEM;
		goto unmap_out;
	}

	err = page_map_create(hv_page_table, config_address & PAGE_MASK,
			      total_size, FOREIGN_MAPPING_BASE,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS);
	if (err)
		goto unmap_out;

	err = check_mem_regions(cfg);
	if (err)
		goto unmap_out;

	cell_pages = PAGE_ALIGN(sizeof(*cell)) / PAGE_SIZE;
	cell = page_alloc(&mem_pool, cell_pages);
	if (!cell) {
		err = -ENOMEM;
		goto unmap_out;
	}

	err = cell_init(cell, cfg, true);
	if (err)
		goto err_free_cell;

	/* don't assign the CPU we are currently running on */
	if (cpu_data->cpu_id <= cell->cpu_set->max_cpu_id &&
	    test_bit(cpu_data->cpu_id, cell->cpu_set->bitmap)) {
		err = -EBUSY;
		goto err_free_cpu_set;
	}

	shrinking_set = cpu_data->cell->cpu_set;

	/* shrinking set must be super-set of new cell's cpu set */
	if (shrinking_set->max_cpu_id < cell->cpu_set->max_cpu_id) {
		err = -EINVAL;
		goto err_free_cpu_set;
	}
	for_each_cpu(cpu, cell->cpu_set)
		if (!test_bit(cpu, shrinking_set->bitmap)) {
			err = -EINVAL;
			goto err_free_cpu_set;
		}

	for_each_cpu(cpu, cell->cpu_set)
		clear_bit(cpu, shrinking_set->bitmap);

	err = arch_cell_create(cpu_data, cell, cfg);
	if (err)
		goto err_restore_cpu_set;

	last = cell_list;
	while (last->next)
		last = last->next;
	last->next = cell;

	/* update cell references and clean up before releasing the cpus of
	 * the new cell */
	for_each_cpu(cpu, cell->cpu_set)
		per_cpu(cpu)->cell = cell;

	printk("Created cell \"%s\"\n", cell->name);

	page_map_destroy(hv_page_table, FOREIGN_MAPPING_BASE, total_size,
			 PAGE_DIR_LEVELS);
	page_map_dump_stats("after cell creation");

	for_each_cpu(cpu, cell->cpu_set)
		arch_reset_cpu(cpu);

resume_out:
	cell_resume(cpu_data);

	return err;

err_restore_cpu_set:
	for_each_cpu(cpu, cell->cpu_set)
		set_bit(cpu, shrinking_set->bitmap);
err_free_cpu_set:
	destroy_cpu_set(cell);
err_free_cell:
	page_free(&mem_pool, cell, cell_pages);
unmap_out:
	page_map_destroy(hv_page_table, FOREIGN_MAPPING_BASE, total_size,
			 PAGE_DIR_LEVELS);
	goto resume_out;
}

int shutdown(struct per_cpu *cpu_data)
{
	static bool shutdown_started;
	struct cell *cell = cell_list->next;
	unsigned int this_cpu = cpu_data->cpu_id;
	unsigned int cpu;

	// TODO: access control

	spin_lock(&shutdown_lock);

	if (!shutdown_started) {
		shutdown_started = true;

		printk("Shutting down hypervisor\n");

		while (cell) {
			printk(" Closing cell \"%s\"\n", cell->name);

			for_each_cpu(cpu, cell->cpu_set) {
				printk("  Releasing CPU %d\n", cpu);
				arch_shutdown_cpu(cpu);
			}
			cell = cell->next;
		}

		printk(" Closing Linux cell \"%s\"\n", cell_list->name);
	}
	printk("  Releasing CPU %d\n", this_cpu);

	spin_unlock(&shutdown_lock);

	return 0;
}
