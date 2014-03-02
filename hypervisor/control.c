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

#define for_each_cell(c)	for (c = &linux_cell; c; c = c->next)

unsigned int next_cpu(unsigned int cpu, struct cpu_set *cpu_set, int exception)
{
	do
		cpu++;
	while (cpu <= cpu_set->max_cpu_id &&
	       (cpu == exception || !test_bit(cpu, cpu_set->bitmap)));
	return cpu;
}

static void cell_suspend(struct cell *cell, struct per_cpu *cpu_data)
{
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
	for_each_cell(cell)
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

	for_each_cell(cell)
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
		    mem->flags & ~JAILHOUSE_MEM_VALID_FLAGS) {
			printk("FATAL: Invalid memory bar (%p, %p, %p, %x)\n",
			       mem->phys_start, mem->virt_start, mem->size,
			       mem->flags);
			return -EINVAL;
		}
	}
	return 0;
}

int cell_create(struct per_cpu *cpu_data, unsigned long config_address)
{
	unsigned long mapping_addr = TEMPORARY_MAPPING_CPU_BASE(cpu_data);
	unsigned long cfg_header_size, cfg_total_size;
	struct jailhouse_cell_desc *cfg;
	struct cpu_set *shrinking_set;
	unsigned int cell_pages, cpu;
	struct cell *cell, *last;
	int err;

	/* We do not support creation over non-Linux cells so far. */
	if (cpu_data->cell != &linux_cell)
		return -EPERM;

	cell_suspend(&linux_cell, cpu_data);

	cfg_header_size = (config_address & ~PAGE_MASK) +
		sizeof(struct jailhouse_cell_desc);

	err = page_map_create(&hv_paging_structs, config_address & PAGE_MASK,
			      cfg_header_size, mapping_addr,
			      PAGE_READONLY_FLAGS, PAGE_MAP_NON_COHERENT);
	if (err)
		goto err_resume;

	cfg = (struct jailhouse_cell_desc *)(mapping_addr +
					     (config_address & ~PAGE_MASK));
	cfg_total_size = jailhouse_cell_config_size(cfg);
	if (cfg_total_size > NUM_TEMPORARY_PAGES * PAGE_SIZE) {
		err = -E2BIG;
		goto err_resume;
	}

	if (cell_find(cfg->name)) {
		err = -EEXIST;
		goto err_resume;
	}

	err = page_map_create(&hv_paging_structs, config_address & PAGE_MASK,
			      cfg_total_size, mapping_addr,
			      PAGE_READONLY_FLAGS, PAGE_MAP_NON_COHERENT);
	if (err)
		goto err_resume;

	err = check_mem_regions(cfg);
	if (err)
		goto err_resume;

	cell_pages = PAGE_ALIGN(sizeof(*cell) + cfg_total_size) / PAGE_SIZE;
	cell = page_alloc(&mem_pool, cell_pages);
	if (!cell) {
		err = -ENOMEM;
		goto err_resume;
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
		err = -EBUSY;
		goto err_free_cpu_set;
	}
	for_each_cpu(cpu, cell->cpu_set)
		if (!test_bit(cpu, shrinking_set->bitmap)) {
			err = -EBUSY;
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

	cell_resume(cpu_data);

	return cell->id;

err_restore_cpu_set:
	for_each_cpu(cpu, cell->cpu_set)
		set_bit(cpu, shrinking_set->bitmap);
err_free_cpu_set:
	destroy_cpu_set(cell);
err_free_cell:
	page_free(&mem_pool, cell, cell_pages);

err_resume:
	cell_resume(cpu_data);

	return err;
}

static bool cell_shutdown_ok(struct cell *cell)
{
	volatile u32 *reply = &cell->comm_page.comm_region.reply_from_cell;
	volatile u32 *cell_status = &cell->comm_page.comm_region.cell_status;

	if (cell->config->flags & JAILHOUSE_CELL_UNMANAGED_EXIT)
		return true;

	jailhouse_send_msg_to_cell(&cell->comm_page.comm_region,
				   JAILHOUSE_MSG_SHUTDOWN_REQUESTED);

	while (*reply != JAILHOUSE_MSG_SHUTDOWN_DENIED) {
		if (*reply == JAILHOUSE_MSG_SHUTDOWN_OK ||
		    *cell_status == JAILHOUSE_CELL_SHUT_DOWN ||
		    *cell_status == JAILHOUSE_CELL_FAILED)
			return true;
		cpu_relax();
	}
	return false;
}

static bool address_in_region(unsigned long addr,
			      const struct jailhouse_memory *region)
{
	return addr >= region->phys_start &&
	       addr < (region->phys_start + region->size);
}

static void remap_to_linux(const struct jailhouse_memory *mem)
{
	const struct jailhouse_memory *linux_mem =
		jailhouse_cell_mem_regions(linux_cell.config);
	struct jailhouse_memory overlap;
	int n;

	for (n = 0; n < linux_cell.config->num_memory_regions;
	     n++, linux_mem++) {
		if (address_in_region(mem->phys_start, linux_mem)) {
			overlap.phys_start = mem->phys_start;
			overlap.size = linux_mem->size -
				(overlap.phys_start - linux_mem->phys_start);
			if (overlap.size > mem->size)
				overlap.size = mem->size;
		} else if (address_in_region(linux_mem->phys_start, mem)) {
			overlap.phys_start = linux_mem->phys_start;
			overlap.size = mem->size -
				(overlap.phys_start - mem->phys_start);
			if (overlap.size > linux_mem->size)
				overlap.size = linux_mem->size;
		} else
			continue;

		overlap.virt_start = linux_mem->virt_start +
			overlap.phys_start - linux_mem->phys_start;
		overlap.flags = linux_mem->flags;

		if (arch_map_memory_region(&linux_cell, &overlap) != 0)
			printk("WARNING: Failed to re-assign memory region "
			       "to Linux cell\n");
	}
}

int cell_destroy(struct per_cpu *cpu_data, unsigned long id)
{
	const struct jailhouse_memory *mem;
	struct cell *cell, *previous;
	unsigned int cpu, n;
	int err = 0;

	/* We do not support destruction over non-Linux cells so far. */
	if (cpu_data->cell != &linux_cell)
		return -EPERM;

	cell_suspend(&linux_cell, cpu_data);

	for_each_cell(cell)
		if (cell->id == id)
			break;
	if (!cell) {
		err = -ENOENT;
		goto resume_out;
	}

	/* Linux cell cannot be destroyed */
	if (cell == &linux_cell) {
		err = -EINVAL;
		goto resume_out;
	}

	if (!cell_shutdown_ok(cell)) {
		err = -EPERM;
		goto resume_out;
	}

	cell_suspend(cell, cpu_data);

	printk("Closing cell \"%s\"\n", cell->config->name);

	for_each_cpu(cpu, cell->cpu_set) {
		printk(" Parking CPU %d\n", cpu);
		arch_park_cpu(cpu);

		set_bit(cpu, linux_cell.cpu_set->bitmap);
		per_cpu(cpu)->cell = &linux_cell;
	}

	mem = jailhouse_cell_mem_regions(cell->config);
	for (n = 0; n < cell->config->num_memory_regions; n++, mem++) {
		arch_unmap_memory_region(cell, mem);
		if (!(mem->flags & JAILHOUSE_MEM_COMM_REGION))
			remap_to_linux(mem);
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
	unsigned int this_cpu = cpu_data->cpu_id;
	struct cell *cell;
	unsigned int cpu;
	int state, ret;

	/* We do not support shutdown over non-Linux cells so far. */
	if (cpu_data->cell != &linux_cell)
		return -EPERM;

	spin_lock(&shutdown_lock);

	if (cpu_data->shutdown_state == SHUTDOWN_NONE) {
		state = SHUTDOWN_STARTED;
		for (cell = linux_cell.next; cell; cell = cell->next)
			if (!cell_shutdown_ok(cell))
				state = -EPERM;

		if (state == SHUTDOWN_STARTED) {
			printk("Shutting down hypervisor\n");

			for (cell = linux_cell.next; cell; cell = cell->next) {
				cell_suspend(cell, cpu_data);

				printk("Closing cell \"%s\"\n",
				       cell->config->name);

				for_each_cpu(cpu, cell->cpu_set) {
					printk(" Releasing CPU %d\n", cpu);
					arch_shutdown_cpu(cpu);
				}
			}

			printk("Closing Linux cell \"%s\"\n",
			       linux_cell.config->name);
			arch_shutdown();
		}

		for_each_cpu(cpu, linux_cell.cpu_set)
			per_cpu(cpu)->shutdown_state = state;
	}

	if (cpu_data->shutdown_state == SHUTDOWN_STARTED) {
		printk(" Releasing CPU %d\n", this_cpu);
		ret = 0;
	} else
		ret = cpu_data->shutdown_state;
	cpu_data->shutdown_state = SHUTDOWN_NONE;

	spin_unlock(&shutdown_lock);

	return ret;
}
