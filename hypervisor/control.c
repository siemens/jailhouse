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
	printk("Suspended cell \"%s\"\n", cell->config->name);
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
	for (cell = &linux_cell; cell; cell = cell->next)
		if (cell->id == id) {
			id++;
			goto retry;
		}

	return id;
}

int cell_init(struct cell *cell, bool copy_cpu_set)
{
	const unsigned long *config_cpu_set =
		jailhouse_cell_cpu_set(cell->config);
	unsigned long cpu_set_size = cell->config->cpu_set_size;
	const struct jailhouse_memory *config_ram =
		jailhouse_cell_mem_regions(cell->config);
	struct cpu_set *cpu_set;

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

static struct cell *cell_find(const char *name)
{
	struct cell *cell;

	for (cell = &linux_cell; cell; cell = cell->next)
		if (strcmp(cell->config->name, name) == 0)
			break;
	return cell;
}

int check_mem_regions(const struct jailhouse_cell_desc *config)
{
	const struct jailhouse_memory *mem =
		jailhouse_cell_mem_regions(config);
	unsigned int n;

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
	unsigned long mapping_addr = FOREIGN_MAPPING_BASE +
		cpu_data->cpu_id * PAGE_SIZE * NUM_FOREIGN_PAGES;
	unsigned long cfg_header_size, cfg_total_size;
	struct jailhouse_cell_desc *cfg;
	struct cpu_set *shrinking_set;
	unsigned int cell_pages, cpu;
	struct cell *cell, *last;
	int err;

	cell_suspend(cpu_data);

	cfg_header_size = (config_address & ~PAGE_MASK) +
		sizeof(struct jailhouse_cell_desc);

	err = page_map_create(hv_page_table, config_address & PAGE_MASK,
			      cfg_header_size, mapping_addr,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS, PAGE_MAP_NON_COHERENT);
	if (err)
		goto resume_out;

	cfg = (struct jailhouse_cell_desc *)(mapping_addr +
					     (config_address & ~PAGE_MASK));
	cfg_total_size = jailhouse_cell_config_size(cfg);
	if (cfg_total_size > NUM_FOREIGN_PAGES * PAGE_SIZE) {
		err = -E2BIG;
		goto resume_out;
	}

	if (cell_find(cfg->name)) {
		err = -EEXIST;
		goto resume_out;
	}

	err = page_map_create(hv_page_table, config_address & PAGE_MASK,
			      cfg_total_size, mapping_addr,
			      PAGE_READONLY_FLAGS, PAGE_DEFAULT_FLAGS,
			      PAGE_DIR_LEVELS, PAGE_MAP_NON_COHERENT);
	if (err)
		goto resume_out;

	err = check_mem_regions(cfg);
	if (err)
		goto resume_out;

	cell_pages = PAGE_ALIGN(sizeof(*cell) + cfg_total_size) / PAGE_SIZE;
	cell = page_alloc(&mem_pool, cell_pages);
	if (!cell) {
		err = -ENOMEM;
		goto resume_out;
	}

	cell->data_pages = cell_pages;
	cell->config = ((void *)cell) + sizeof(*cell);
	memcpy(cell->config, cfg, cfg_total_size);

	err = cell_init(cell, true);
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

	err = arch_cell_create(cpu_data, cell);
	if (err)
		goto err_restore_cpu_set;

	last = &linux_cell;
	while (last->next)
		last = last->next;
	last->next = cell;

	/* update cell references and clean up before releasing the cpus of
	 * the new cell */
	for_each_cpu(cpu, cell->cpu_set)
		per_cpu(cpu)->cell = cell;

	printk("Created cell \"%s\"\n", cell->config->name);

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
	goto resume_out;
}

int cell_destroy(struct per_cpu *cpu_data, unsigned long name_address)
{
	unsigned long mapping_addr = FOREIGN_MAPPING_BASE +
		cpu_data->cpu_id * PAGE_SIZE * NUM_FOREIGN_PAGES;
	struct cell *cell, *previous;
	unsigned long name_size;
	const char *name;
	unsigned int cpu;
	int err = 0;

	// TODO: access control

	/* We do not support destruction over non-Linux cells so far */
	if (cpu_data->cell != &linux_cell)
		return -EINVAL;

	cell_suspend(cpu_data);

	name_size = (name_address & ~PAGE_MASK) + JAILHOUSE_CELL_NAME_MAXLEN;

	err = page_map_create(hv_page_table, name_address & PAGE_MASK,
			      name_size, mapping_addr, PAGE_READONLY_FLAGS,
			      PAGE_DEFAULT_FLAGS, PAGE_DIR_LEVELS,
			      PAGE_MAP_NON_COHERENT);
	if (err)
		goto resume_out;

	name = (const char *)(mapping_addr + (name_address & ~PAGE_MASK));

	cell = cell_find(name);
	if (!cell) {
		err = -ENOENT;
		goto resume_out;
	}

	/* Linux cell cannot be destroyed */
	if (cell == &linux_cell) {
		err = -EINVAL;
		goto resume_out;
	}

	printk("Closing cell \"%s\"\n", name);

	for_each_cpu(cpu, cell->cpu_set) {
		printk(" Parking CPU %d\n", cpu);
		arch_park_cpu(cpu);

		set_bit(cpu, linux_cell.cpu_set->bitmap);
		per_cpu(cpu)->cell = &linux_cell;
	}

	arch_cell_destroy(cpu_data, cell);

	previous = &linux_cell;
	while (previous->next != cell)
		previous = previous->next;
	previous->next = cell->next;

	page_free(&mem_pool, cell, cell->data_pages);
	page_map_dump_stats("after cell destruction");

resume_out:
	cell_resume(cpu_data);

	return err;
}

int shutdown(struct per_cpu *cpu_data)
{
	static bool shutdown_started;
	struct cell *cell = linux_cell.next;
	unsigned int this_cpu = cpu_data->cpu_id;
	unsigned int cpu;

	// TODO: access control

	spin_lock(&shutdown_lock);

	if (!shutdown_started) {
		shutdown_started = true;

		printk("Shutting down hypervisor\n");

		while (cell) {
			printk(" Closing cell \"%s\"\n", cell->config->name);

			for_each_cpu(cpu, cell->cpu_set) {
				printk("  Releasing CPU %d\n", cpu);
				arch_shutdown_cpu(cpu);
			}
			cell = cell->next;
		}

		printk(" Closing Linux cell \"%s\"\n",
		       linux_cell.config->name);
	}
	printk("  Releasing CPU %d\n", this_cpu);

	spin_unlock(&shutdown_lock);

	return 0;
}
