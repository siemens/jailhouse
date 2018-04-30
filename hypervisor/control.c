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

#include <jailhouse/entry.h>
#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <jailhouse/paging.h>
#include <jailhouse/processor.h>
#include <jailhouse/string.h>
#include <jailhouse/unit.h>
#include <jailhouse/utils.h>
#include <asm/bitops.h>
#include <asm/spinlock.h>

enum msg_type {MSG_REQUEST, MSG_INFORMATION};
enum failure_mode {ABORT_ON_ERROR, WARN_ON_ERROR};
enum management_task {CELL_START, CELL_SET_LOADABLE, CELL_DESTROY};

/** System configuration as used while activating the hypervisor. */
struct jailhouse_system *system_config;
/** State structure of the root cell. @ingroup Control */
struct cell root_cell;

static DEFINE_SPINLOCK(shutdown_lock);
static unsigned int num_cells = 1;

volatile unsigned long panic_in_progress;
unsigned long panic_cpu = -1;

/**
 * CPU set iterator.
 * @param cpu		Previous CPU ID.
 * @param cpu_set	CPU set to iterate over.
 * @param exception	CPU ID to skip if it is contained.
 *
 * @return Next CPU ID in the set.
 *
 * @note For internal use only. Use for_each_cpu() or for_each_cpu_except()
 * instead.
 */
unsigned int next_cpu(unsigned int cpu, struct cpu_set *cpu_set, int exception)
{
	do
		cpu++;
	while (cpu <= cpu_set->max_cpu_id &&
	       (cpu == exception || !test_bit(cpu, cpu_set->bitmap)));
	return cpu;
}

/**
 * Check if a CPU ID is contained in the system's CPU set, i.e. the initial CPU
 * set of the root cell.
 * @param cpu_id	CPU ID to check.
 *
 * @return True if CPU ID is valid.
 */
bool cpu_id_valid(unsigned long cpu_id)
{
	const unsigned long *system_cpu_set =
		jailhouse_cell_cpu_set(&system_config->root_cell);

	return (cpu_id < system_config->root_cell.cpu_set_size * 8 &&
		test_bit(cpu_id, system_cpu_set));
}

/*
 * Suspend all CPUs assigned to the cell except the one executing
 * the function (if it is in the cell's CPU set) to prevent races.
 */
static void cell_suspend(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu_except(cpu, cell->cpu_set, this_cpu_id())
		arch_suspend_cpu(cpu);
}

static void cell_resume(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu_except(cpu, cell->cpu_set, this_cpu_id())
		arch_resume_cpu(cpu);
}

/**
 * Deliver a message to cell and wait for the reply.
 * @param cell		Target cell.
 * @param message	Message code to be sent (JAILHOUSE_MSG_*).
 * @param type		Message type, defines the valid replies.
 *
 * @return True if a request message was approved or reception of an
 * 	   informational message was acknowledged by the target cell. It also
 * 	   returns true if the target cell does not support an active
 * 	   communication region, is shut down or in failed state.
 *	   In case of timeout (if enabled) it also stops the cell and put it
 *	   in failed state.
 *	   Returns false on request denial or invalid replies.
 */
static bool cell_exchange_message(struct cell *cell, u32 message,
				  enum msg_type type)
{
	u64 timeout = cell->config->msg_reply_timeout;

	if (cell->config->flags & JAILHOUSE_CELL_PASSIVE_COMMREG)
		return true;

	jailhouse_send_msg_to_cell(&cell->comm_page.comm_region, message);

	while (1) {
		u32 reply = cell->comm_page.comm_region.reply_from_cell;
		u32 cell_state = cell->comm_page.comm_region.cell_state;

		if (cell_state == JAILHOUSE_CELL_SHUT_DOWN ||
		    cell_state == JAILHOUSE_CELL_FAILED)
			return true;

		if ((type == MSG_REQUEST &&
		     reply == JAILHOUSE_MSG_REQUEST_APPROVED) ||
		    (type == MSG_INFORMATION &&
		     reply == JAILHOUSE_MSG_RECEIVED))
			return true;

		if (reply != JAILHOUSE_MSG_NONE)
			return false;

		if (cell->config->msg_reply_timeout > 0 && --timeout == 0) {
			printk("Timeout expired while waiting for reply from "
			       "target cell\n");
			cell_suspend(cell);
			cell->comm_page.comm_region.cell_state =
				JAILHOUSE_CELL_FAILED;
			return true;
		}

		cpu_relax();
	}
}

static bool cell_reconfig_ok(struct cell *excluded_cell)
{
	struct cell *cell;

	for_each_non_root_cell(cell)
		if (cell != excluded_cell &&
		    cell->comm_page.comm_region.cell_state ==
				JAILHOUSE_CELL_RUNNING_LOCKED)
			return false;
	return true;
}

static void cell_reconfig_completed(void)
{
	struct cell *cell;

	for_each_non_root_cell(cell)
		cell_exchange_message(cell, JAILHOUSE_MSG_RECONFIG_COMPLETED,
				      MSG_INFORMATION);
}

/**
 * Initialize a new cell.
 * @param cell	Cell to be initialized.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @note Uninitialized fields of the cell data structure must be zeroed.
 */
int cell_init(struct cell *cell)
{
	const unsigned long *config_cpu_set =
		jailhouse_cell_cpu_set(cell->config);
	unsigned long cpu_set_size = cell->config->cpu_set_size;
	struct cpu_set *cpu_set;
	int err;

	if (cpu_set_size > PAGE_SIZE)
		return trace_error(-EINVAL);
	if (cpu_set_size > sizeof(cell->small_cpu_set.bitmap)) {
		cpu_set = page_alloc(&mem_pool, 1);
		if (!cpu_set)
			return -ENOMEM;
	} else {
		cpu_set = &cell->small_cpu_set;
	}
	cpu_set->max_cpu_id = cpu_set_size * 8 - 1;
	memcpy(cpu_set->bitmap, config_cpu_set, cpu_set_size);

	cell->cpu_set = cpu_set;

	err = mmio_cell_init(cell);
	if (err && cell->cpu_set != &cell->small_cpu_set)
		page_free(&mem_pool, cell->cpu_set, 1);

	return err;
}

static void cell_exit(struct cell *cell)
{
	mmio_cell_exit(cell);

	if (cell->cpu_set != &cell->small_cpu_set)
		page_free(&mem_pool, cell->cpu_set, 1);
}

/**
 * Apply system configuration changes.
 * @param cell_added_removed	Cell that was added or removed to/from the
 * 				system or NULL.
 *
 * @see arch_config_commit
 * @see pci_config_commit
 */
void config_commit(struct cell *cell_added_removed)
{
	arch_flush_cell_vcpu_caches(&root_cell);
	if (cell_added_removed && cell_added_removed != &root_cell)
		arch_flush_cell_vcpu_caches(cell_added_removed);

	arch_config_commit(cell_added_removed);
	pci_config_commit(cell_added_removed);
}

static bool address_in_region(unsigned long addr,
			      const struct jailhouse_memory *region)
{
	return addr >= region->phys_start &&
	       addr < (region->phys_start + region->size);
}

static int unmap_from_root_cell(const struct jailhouse_memory *mem)
{
	/*
	 * arch_unmap_memory_region and mmio_subpage_unregister use the
	 * virtual address of the memory region for their job. As only the root
	 * cell has a guaranteed 1:1 mapping, make a copy where we ensure this.
	 */
	struct jailhouse_memory tmp = *mem;

	tmp.virt_start = tmp.phys_start;

	if (JAILHOUSE_MEMORY_IS_SUBPAGE(&tmp)) {
		mmio_subpage_unregister(&root_cell, &tmp);
		return 0;
	}

	return arch_unmap_memory_region(&root_cell, &tmp);
}

static int remap_to_root_cell(const struct jailhouse_memory *mem,
			      enum failure_mode mode)
{
	const struct jailhouse_memory *root_mem;
	struct jailhouse_memory overlap;
	unsigned int n;
	int err = 0;

	for_each_mem_region(root_mem, root_cell.config, n) {
		if (address_in_region(mem->phys_start, root_mem)) {
			overlap.phys_start = mem->phys_start;
			overlap.size = root_mem->size -
				(overlap.phys_start - root_mem->phys_start);
			if (overlap.size > mem->size)
				overlap.size = mem->size;
		} else if (address_in_region(root_mem->phys_start, mem)) {
			overlap.phys_start = root_mem->phys_start;
			overlap.size = mem->size -
				(overlap.phys_start - mem->phys_start);
			if (overlap.size > root_mem->size)
				overlap.size = root_mem->size;
		} else
			continue;

		overlap.virt_start = root_mem->virt_start +
			overlap.phys_start - root_mem->phys_start;
		overlap.flags = root_mem->flags;

		if (JAILHOUSE_MEMORY_IS_SUBPAGE(&overlap))
			err = mmio_subpage_register(&root_cell, &overlap);
		else
			err = arch_map_memory_region(&root_cell, &overlap);
		if (err) {
			if (mode == ABORT_ON_ERROR)
				break;
			printk("WARNING: Failed to re-assign memory region "
			       "to root cell\n");
		}
	}
	return err;
}

static void cell_destroy_internal(struct per_cpu *cpu_data, struct cell *cell)
{
	const struct jailhouse_memory *mem;
	unsigned int cpu, n;
	struct unit *unit;

	for_each_cpu(cpu, cell->cpu_set) {
		arch_park_cpu(cpu);

		set_bit(cpu, root_cell.cpu_set->bitmap);
		per_cpu(cpu)->cell = &root_cell;
		per_cpu(cpu)->failed = false;
		memset(per_cpu(cpu)->stats, 0, sizeof(per_cpu(cpu)->stats));
	}

	for_each_mem_region(mem, cell->config, n) {
		if (!JAILHOUSE_MEMORY_IS_SUBPAGE(mem))
			/*
			 * This cannot fail. The region was mapped as a whole
			 * before, thus no hugepages need to be broken up to
			 * unmap it.
			 */
			arch_unmap_memory_region(cell, mem);

		if (!(mem->flags & (JAILHOUSE_MEM_COMM_REGION |
				    JAILHOUSE_MEM_ROOTSHARED)))
			remap_to_root_cell(mem, WARN_ON_ERROR);
	}

	for_each_unit_reverse(unit)
		unit->cell_exit(cell);
	arch_cell_destroy(cell);

	config_commit(cell);

	cell_exit(cell);
}

static int cell_create(struct per_cpu *cpu_data, unsigned long config_address)
{
	unsigned long cfg_page_offs = config_address & ~PAGE_MASK;
	unsigned int cfg_pages, cell_pages, cpu, n;
	const struct jailhouse_memory *mem;
	struct jailhouse_cell_desc *cfg;
	unsigned long cfg_total_size;
	struct cell *cell, *last;
	struct unit *unit;
	void *cfg_mapping;
	int err;

	/* We do not support creation over non-root cells. */
	if (cpu_data->cell != &root_cell)
		return -EPERM;

	cell_suspend(&root_cell);

	if (!cell_reconfig_ok(NULL)) {
		err = -EPERM;
		goto err_resume;
	}

	cfg_pages = PAGES(cfg_page_offs + sizeof(struct jailhouse_cell_desc));
	cfg_mapping = paging_get_guest_pages(NULL, config_address, cfg_pages,
					     PAGE_READONLY_FLAGS);
	if (!cfg_mapping) {
		err = -ENOMEM;
		goto err_resume;
	}

	cfg = (struct jailhouse_cell_desc *)(cfg_mapping + cfg_page_offs);

	for_each_cell(cell)
		/*
		 * No bound checking needed, thus strcmp is safe here because
		 * sizeof(cell->config->name) == sizeof(cfg->name) and
		 * cell->config->name is guaranteed to be null-terminated.
		 */
		if (strcmp(cell->config->name, cfg->name) == 0 ||
		    cell->config->id == cfg->id) {
			err = -EEXIST;
			goto err_resume;
		}

	cfg_total_size = jailhouse_cell_config_size(cfg);
	cfg_pages = PAGES(cfg_page_offs + cfg_total_size);
	if (cfg_pages > NUM_TEMPORARY_PAGES) {
		err = trace_error(-E2BIG);
		goto err_resume;
	}

	if (!paging_get_guest_pages(NULL, config_address, cfg_pages,
				    PAGE_READONLY_FLAGS)) {
		err = -ENOMEM;
		goto err_resume;
	}

	cell_pages = PAGES(sizeof(*cell) + cfg_total_size);
	cell = page_alloc(&mem_pool, cell_pages);
	if (!cell) {
		err = -ENOMEM;
		goto err_resume;
	}

	cell->data_pages = cell_pages;
	cell->config = ((void *)cell) + sizeof(*cell);
	memcpy(cell->config, cfg, cfg_total_size);

	err = cell_init(cell);
	if (err)
		goto err_free_cell;

	/* don't assign the CPU we are currently running on */
	if (cell_owns_cpu(cell, cpu_data->cpu_id)) {
		err = trace_error(-EBUSY);
		goto err_cell_exit;
	}

	/* the root cell's cpu set must be super-set of new cell's set */
	for_each_cpu(cpu, cell->cpu_set)
		if (!cell_owns_cpu(&root_cell, cpu)) {
			err = trace_error(-EBUSY);
			goto err_cell_exit;
		}

	err = arch_cell_create(cell);
	if (err)
		goto err_cell_exit;

	for_each_unit(unit) {
		err = unit->cell_init(cell);
		if (err) {
			for_each_unit_before_reverse(unit, unit)
				unit->cell_exit(cell);
			goto err_arch_destroy;
		}
	}

	/*
	 * Shrinking: the new cell's CPUs are parked, then removed from the root
	 * cell, assigned to the new cell and get their stats cleared.
	 */
	for_each_cpu(cpu, cell->cpu_set) {
		arch_park_cpu(cpu);

		clear_bit(cpu, root_cell.cpu_set->bitmap);
		per_cpu(cpu)->cell = cell;
		memset(per_cpu(cpu)->stats, 0, sizeof(per_cpu(cpu)->stats));
	}

	/*
	 * Unmap the cell's memory regions from the root cell and map them to
	 * the new cell instead.
	 */
	for_each_mem_region(mem, cell->config, n) {
		/*
		 * Unmap exceptions:
		 *  - the communication region is not backed by root memory
		 *  - regions that may be shared with the root cell
		 */
		if (!(mem->flags & (JAILHOUSE_MEM_COMM_REGION |
				    JAILHOUSE_MEM_ROOTSHARED))) {
			err = unmap_from_root_cell(mem);
			if (err)
				goto err_destroy_cell;
		}

		if (JAILHOUSE_MEMORY_IS_SUBPAGE(mem))
			err = mmio_subpage_register(cell, mem);
		else
			err = arch_map_memory_region(cell, mem);
		if (err)
			goto err_destroy_cell;
	}

	config_commit(cell);

	cell->comm_page.comm_region.cell_state = JAILHOUSE_CELL_SHUT_DOWN;

	last = &root_cell;
	while (last->next)
		last = last->next;
	last->next = cell;
	num_cells++;

	cell_reconfig_completed();

	printk("Created cell \"%s\"\n", cell->config->name);

	paging_dump_stats("after cell creation");

	cell_resume(&root_cell);

	return 0;

err_destroy_cell:
	cell_destroy_internal(cpu_data, cell);
	/* cell_destroy_internal already calls arch_cell_destroy & cell_exit */
	goto err_free_cell;
err_arch_destroy:
	arch_cell_destroy(cell);
err_cell_exit:
	cell_exit(cell);
err_free_cell:
	page_free(&mem_pool, cell, cell_pages);
err_resume:
	cell_resume(&root_cell);

	return err;
}

static bool cell_shutdown_ok(struct cell *cell)
{
	return cell_exchange_message(cell, JAILHOUSE_MSG_SHUTDOWN_REQUEST,
				     MSG_REQUEST);
}

static int cell_management_prologue(enum management_task task,
				    struct per_cpu *cpu_data, unsigned long id,
				    struct cell **cell_ptr)
{
	/* We do not support management commands over non-root cells. */
	if (cpu_data->cell != &root_cell)
		return -EPERM;

	cell_suspend(&root_cell);

	for_each_cell(*cell_ptr)
		if ((*cell_ptr)->config->id == id)
			break;

	if (!*cell_ptr) {
		cell_resume(&root_cell);
		return -ENOENT;
	}

	/* root cell cannot be managed */
	if (*cell_ptr == &root_cell) {
		cell_resume(&root_cell);
		return -EINVAL;
	}

	if ((task == CELL_DESTROY && !cell_reconfig_ok(*cell_ptr)) ||
	    !cell_shutdown_ok(*cell_ptr)) {
		cell_resume(&root_cell);
		return -EPERM;
	}

	cell_suspend(*cell_ptr);

	return 0;
}

static int cell_start(struct per_cpu *cpu_data, unsigned long id)
{
	struct jailhouse_comm_region *comm_region;
	const struct jailhouse_memory *mem;
	unsigned int cpu, n;
	struct cell *cell;
	int err;

	err = cell_management_prologue(CELL_START, cpu_data, id, &cell);
	if (err)
		return err;

	if (cell->loadable) {
		/* unmap all loadable memory regions from the root cell */
		for_each_mem_region(mem, cell->config, n)
			if (mem->flags & JAILHOUSE_MEM_LOADABLE) {
				err = unmap_from_root_cell(mem);
				if (err)
					goto out_resume;
			}

		config_commit(NULL);

		cell->loadable = false;
	}

	/*
	 * Present a consistent Communication Region state to the cell. Zero the
	 * whole region as it might be dirty. This implies:
	 *   - cell_state = JAILHOUSE_CELL_RUNNING (0)
	 *   - msg_to_cell = JAILHOUSE_MSG_NONE (0)
	 */
	comm_region = &cell->comm_page.comm_region;
	memset(&cell->comm_page, 0, sizeof(cell->comm_page));

	comm_region->revision = COMM_REGION_ABI_REVISION;
	memcpy(comm_region->signature, COMM_REGION_MAGIC,
	       sizeof(comm_region->signature));

	pci_cell_reset(cell);
	arch_cell_reset(cell);

	for_each_cpu(cpu, cell->cpu_set) {
		per_cpu(cpu)->failed = false;
		arch_reset_cpu(cpu);
	}

	printk("Started cell \"%s\"\n", cell->config->name);

out_resume:
	cell_resume(&root_cell);

	return err;
}

static int cell_set_loadable(struct per_cpu *cpu_data, unsigned long id)
{
	const struct jailhouse_memory *mem;
	unsigned int cpu, n;
	struct cell *cell;
	int err;

	err = cell_management_prologue(CELL_SET_LOADABLE, cpu_data, id, &cell);
	if (err)
		return err;

	/*
	 * Unconditionally park so that the target cell's CPUs don't stay in
	 * suspension mode.
	 */
	for_each_cpu(cpu, cell->cpu_set) {
		per_cpu(cpu)->failed = false;
		arch_park_cpu(cpu);
	}

	if (cell->loadable)
		goto out_resume;

	cell->comm_page.comm_region.cell_state = JAILHOUSE_CELL_SHUT_DOWN;
	cell->loadable = true;

	/* map all loadable memory regions into the root cell */
	for_each_mem_region(mem, cell->config, n)
		if (mem->flags & JAILHOUSE_MEM_LOADABLE) {
			err = remap_to_root_cell(mem, ABORT_ON_ERROR);
			if (err)
				goto out_resume;
		}

	config_commit(NULL);

	printk("Cell \"%s\" can be loaded\n", cell->config->name);

out_resume:
	cell_resume(&root_cell);

	return err;
}

static int cell_destroy(struct per_cpu *cpu_data, unsigned long id)
{
	struct cell *cell, *previous;
	int err;

	err = cell_management_prologue(CELL_DESTROY, cpu_data, id, &cell);
	if (err)
		return err;

	printk("Closing cell \"%s\"\n", cell->config->name);

	cell_destroy_internal(cpu_data, cell);

	previous = &root_cell;
	while (previous->next != cell)
		previous = previous->next;
	previous->next = cell->next;
	num_cells--;

	page_free(&mem_pool, cell, cell->data_pages);
	paging_dump_stats("after cell destruction");

	cell_reconfig_completed();

	cell_resume(&root_cell);

	return 0;
}

static int cell_get_state(struct per_cpu *cpu_data, unsigned long id)
{
	struct cell *cell;

	if (cpu_data->cell != &root_cell)
		return -EPERM;

	/*
	 * We do not need explicit synchronization with cell_create/destroy
	 * because their cell_suspend(root_cell) will not return before we left
	 * this hypercall.
	 */
	for_each_cell(cell)
		if (cell->config->id == id) {
			u32 state = cell->comm_page.comm_region.cell_state;

			switch (state) {
			case JAILHOUSE_CELL_RUNNING:
			case JAILHOUSE_CELL_RUNNING_LOCKED:
			case JAILHOUSE_CELL_SHUT_DOWN:
			case JAILHOUSE_CELL_FAILED:
			case JAILHOUSE_CELL_FAILED_COMM_REV:
				return state;
			default:
				return -EINVAL;
			}
		}
	return -ENOENT;
}

/**
 * Perform all CPU-unrelated hypervisor shutdown steps.
 */
void shutdown(void)
{
	struct unit *unit;

	pci_prepare_handover();
	arch_prepare_shutdown();

	for_each_unit_reverse(unit)
		unit->shutdown();
}

static int hypervisor_disable(struct per_cpu *cpu_data)
{
	static volatile unsigned int waiting_cpus;
	static bool do_common_shutdown;
	unsigned int this_cpu = cpu_data->cpu_id;
	unsigned int cpu;
	int state, ret;

	/* We do not support shutdown over non-root cells. */
	if (cpu_data->cell != &root_cell)
		return -EPERM;

	/*
	 * This may race against another root cell CPU invoking a different
	 * management hypercall (cell create, set loadable, start, destroy).
	 * Those suspend the root cell to protect against concurrent requests.
	 * We can't do this here because all root cell CPUs will invoke this
	 * function, and cell_suspend doesn't support such a scenario. We are
	 * safe nevertheless because we only need to see a consistent num_cells
	 * that is not increasing anymore once the shutdown was started:
	 *
	 * If another CPU in a management hypercall already called cell_suspend,
	 * it is now waiting for this CPU to react. In this case, we see
	 * num_cells prior to any change, can start the shutdown if it is 1, and
	 * will prevent the other CPU from changing it anymore. This is because
	 * we are taking one CPU away from the hypervisor when leaving shutdown.
	 * This will lock up the root cell (which is violating the hypercall
	 * protocol), but only if it was the last cell.
	 *
	 * If the other CPU already returned from cell_suspend, we cannot be
	 * running in parallel before that CPU releases the root cell again via
	 * cell_resume. In that case, we will see the result of the change.
	 *
	 * shutdown_lock is here to protect shutdown_state, waiting_cpus and
	 * do_common_shutdown.
	 */
	spin_lock(&shutdown_lock);

	if (cpu_data->shutdown_state == SHUTDOWN_NONE) {
		state = num_cells == 1 ? SHUTDOWN_STARTED : -EBUSY;
		for_each_cpu(cpu, root_cell.cpu_set)
			per_cpu(cpu)->shutdown_state = state;
	}

	if (cpu_data->shutdown_state == SHUTDOWN_STARTED) {
		do_common_shutdown = true;
		waiting_cpus++;
		ret = 0;
	} else {
		ret = cpu_data->shutdown_state;
		cpu_data->shutdown_state = SHUTDOWN_NONE;
	}

	spin_unlock(&shutdown_lock);

	if (ret < 0)
		return ret;

	/*
	 * The shutdown will change hardware behavior, and we  have to avoid
	 * that one CPU already turns it to native mode while another makes use
	 * of it or runs into a hypervisor trap. This barrier prevents such
	 * scenarios.
	 */
	while (waiting_cpus < hypervisor_header.online_cpus)
		cpu_relax();

	spin_lock(&shutdown_lock);

	if (do_common_shutdown) {
		/*
		 * The first CPU to get here changes common settings to native.
		 */
		printk("Shutting down hypervisor\n");
		shutdown();
		do_common_shutdown = false;
	}
	printk(" Releasing CPU %d\n", this_cpu);

	spin_unlock(&shutdown_lock);

	return 0;
}

static long hypervisor_get_info(struct per_cpu *cpu_data, unsigned long type)
{
	switch (type) {
	case JAILHOUSE_INFO_MEM_POOL_SIZE:
		return mem_pool.pages;
	case JAILHOUSE_INFO_MEM_POOL_USED:
		return mem_pool.used_pages;
	case JAILHOUSE_INFO_REMAP_POOL_SIZE:
		return remap_pool.pages;
	case JAILHOUSE_INFO_REMAP_POOL_USED:
		return remap_pool.used_pages;
	case JAILHOUSE_INFO_NUM_CELLS:
		return num_cells;
	default:
		return -EINVAL;
	}
}

static int cpu_get_info(struct per_cpu *cpu_data, unsigned long cpu_id,
			unsigned long type)
{
	if (!cpu_id_valid(cpu_id))
		return -EINVAL;

	/*
	 * We do not need explicit synchronization with cell_destroy because
	 * its cell_suspend(root_cell + this_cell) will not return before we
	 * left this hypercall.
	 */
	if (cpu_data->cell != &root_cell &&
	    !cell_owns_cpu(cpu_data->cell, cpu_id))
		return -EPERM;

	if (type == JAILHOUSE_CPU_INFO_STATE) {
		return per_cpu(cpu_id)->failed ? JAILHOUSE_CPU_FAILED :
			JAILHOUSE_CPU_RUNNING;
	} else if (type >= JAILHOUSE_CPU_INFO_STAT_BASE &&
		type - JAILHOUSE_CPU_INFO_STAT_BASE < JAILHOUSE_NUM_CPU_STATS) {
		type -= JAILHOUSE_CPU_INFO_STAT_BASE;
		return per_cpu(cpu_id)->stats[type] & BIT_MASK(30, 0);
	} else
		return -EINVAL;
}

/**
 * Handle hypercall invoked by a cell.
 * @param code		Hypercall code.
 * @param arg1		First hypercall argument.
 * @param arg2		Seconds hypercall argument.
 *
 * @return Value that shall be passed to the caller of the hypercall on return.
 *
 * @note If @c arg1 and @c arg2 are valid depends on the hypercall code.
 */
long hypercall(unsigned long code, unsigned long arg1, unsigned long arg2)
{
	struct per_cpu *cpu_data = this_cpu_data();

	cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_HYPERCALL]++;

	switch (code) {
	case JAILHOUSE_HC_DISABLE:
		return hypervisor_disable(cpu_data);
	case JAILHOUSE_HC_CELL_CREATE:
		return cell_create(cpu_data, arg1);
	case JAILHOUSE_HC_CELL_START:
		return cell_start(cpu_data, arg1);
	case JAILHOUSE_HC_CELL_SET_LOADABLE:
		return cell_set_loadable(cpu_data, arg1);
	case JAILHOUSE_HC_CELL_DESTROY:
		return cell_destroy(cpu_data, arg1);
	case JAILHOUSE_HC_HYPERVISOR_GET_INFO:
		return hypervisor_get_info(cpu_data, arg1);
	case JAILHOUSE_HC_CELL_GET_STATE:
		return cell_get_state(cpu_data, arg1);
	case JAILHOUSE_HC_CPU_GET_INFO:
		return cpu_get_info(cpu_data, arg1, arg2);
	case JAILHOUSE_HC_DEBUG_CONSOLE_PUTC:
		if (!(cpu_data->cell->config->flags &
		      JAILHOUSE_CELL_DEBUG_CONSOLE))
			return trace_error(-EPERM);
		printk("%c", (char)arg1);
		return 0;
	default:
		return -ENOSYS;
	}
}

/**
 * Stops the current CPU on panic and prevents any execution on it until the
 * system is rebooted.
 *
 * @note This service should be used when facing an unrecoverable error of the
 * hypervisor.
 *
 * @see panic_park
 */
void __attribute__((noreturn)) panic_stop(void)
{
	struct cell *cell = this_cell();

	panic_printk("Stopping CPU %d (Cell: \"%s\")\n", this_cpu_id(),
		     cell && cell->config ? cell->config->name : "<UNSET>");

	if (phys_processor_id() == panic_cpu)
		panic_in_progress = 0;

	arch_panic_stop();
}

/**
 * Parks the current CPU on panic, allowing to restart it by resetting the
 * cell's CPU state.
 *
 * @note This service should be used when facing an error of a cell CPU, e.g. a
 * cell boundary violation.
 *
 * @see panic_stop
 */
void panic_park(void)
{
	struct cell *cell = this_cell();
	bool cell_failed = true;
	unsigned int cpu;

	panic_printk("Parking CPU %d (Cell: \"%s\")\n", this_cpu_id(),
		     cell->config->name);

	this_cpu_data()->failed = true;
	for_each_cpu(cpu, cell->cpu_set)
		if (!per_cpu(cpu)->failed) {
			cell_failed = false;
			break;
		}
	if (cell_failed)
		cell->comm_page.comm_region.cell_state = JAILHOUSE_CELL_FAILED;

	arch_panic_park();

	if (phys_processor_id() == panic_cpu)
		panic_in_progress = 0;
}
