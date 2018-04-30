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

#include <asm/bitops.h>
#include <jailhouse/percpu.h>
#include <jailhouse/cell.h>
#include <jailhouse/cell-config.h>

#define SHUTDOWN_NONE			0
#define SHUTDOWN_STARTED		1

extern volatile unsigned long panic_in_progress;
extern unsigned long panic_cpu;

/**
 * @defgroup Control Control Subsystem
 *
 * The control subsystem provides services for managing cells and the
 * hypervisor during runtime. It implements the hypercall interface and
 * performs the required access control and parameter validation for it.
 *
 * @{
 */

extern struct jailhouse_system *system_config;

unsigned int next_cpu(unsigned int cpu, struct cpu_set *cpu_set,
		      int exception);

/**
 * Get the first CPU in a given set.
 * @param set		CPU set.
 *
 * @return First CPU in set, or max_cpu_id + 1 if the set is empty.
 */
#define first_cpu(set)		next_cpu(-1, (set), -1)

/**
 * Loop-generating macro for iterating over all CPUs in a set.
 * @param cpu		Iteration variable holding the current CPU ID
 * 			(unsigned int).
 * @param set		CPU set to iterate over (struct cpu_set).
 *
 * @see for_each_cpu_except
 */
#define for_each_cpu(cpu, set)	for_each_cpu_except(cpu, set, -1)

/**
 * Loop-generating macro for iterating over all CPUs in a set, except the
 * specified one.
 * @param cpu		Iteration variable holding the current CPU ID
 * 			(unsigned int).
 * @param set		CPU set to iterate over (struct cpu_set).
 * @param exception	CPU to skip if it is part of the set.
 *
 * @see for_each_cpu
 */
#define for_each_cpu_except(cpu, set, exception)		\
	for ((cpu) = -1;					\
	     (cpu) = next_cpu((cpu), (set), (exception)),	\
	     (cpu) <= (set)->max_cpu_id;			\
	    )

/**
 * Loop-generating macro for iterating over all registered cells.
 * @param cell		Iteration variable holding the reference to the current
 * 			cell (struct cell *).
 *
 * @see for_each_non_root_cell
 */
#define for_each_cell(cell)					\
	for ((cell) = &root_cell; (cell); (cell) = (cell)->next)

/**
 * Loop-generating macro for iterating over all registered cells, expect the
 * root cell.
 * @param cell		Iteration variable holding the reference to the current
 * 			cell (struct cell *).
 *
 * @see for_each_cell
 */
#define for_each_non_root_cell(cell) \
	for ((cell) = root_cell.next; (cell); (cell) = (cell)->next)

/**
 * Loop-generating macro for iterating over all memory regions of a
 * configuration.
 * @param mem		Iteration variable holding the reference to the current
 * 			memory region (const struct jailhouse_memory *).
 * @param config	Cell or system configuration containing the regions.
 * @param counter	Helper variable (unsigned int).
 */
#define for_each_mem_region(mem, config, counter)			\
	for ((mem) = jailhouse_cell_mem_regions(config), (counter) = 0;	\
	     (counter) < (config)->num_memory_regions;			\
	     (mem)++, (counter)++)

/**
 * Check if the CPU is assigned to the specified cell.
 * @param cell		Cell the CPU may belong to.
 * @param cpu_id	ID of the CPU.
 *
 * @return True if the CPU is assigned to the cell.
 */
static inline bool cell_owns_cpu(struct cell *cell, unsigned int cpu_id)
{
	return (cpu_id <= cell->cpu_set->max_cpu_id &&
		test_bit(cpu_id, cell->cpu_set->bitmap));
}

bool cpu_id_valid(unsigned long cpu_id);

int cell_init(struct cell *cell);

void config_commit(struct cell *cell_added_removed);

long hypercall(unsigned long code, unsigned long arg1, unsigned long arg2);

void shutdown(void);

void __attribute__((noreturn)) panic_stop(void);
void panic_park(void);

/**
 * Suspend a remote CPU.
 * @param cpu_id	ID of the target CPU.
 *
 * Suspension means that the target CPU is no longer executing cell code or
 * arbitrary hypervisor code. It may actively busy-wait in the hypervisor
 * context, so the suspension time should be kept short.
 *
 * The function waits for the target CPU to enter suspended state.
 *
 * This service can be used to synchronize with other CPUs before performing
 * management tasks.
 *
 * @note This function must not be invoked for the caller's CPU.
 *
 * @see arch_resume_cpu
 * @see arch_reset_cpu
 * @see arch_park_cpu
 */
void arch_suspend_cpu(unsigned int cpu_id);

/**
 * Resume a suspended remote CPU.
 * @param cpu_id	ID of the target CPU.
 *
 * @note This function must not be invoked for the caller's CPU.
 *
 * @see arch_suspend_cpu
 */
void arch_resume_cpu(unsigned int cpu_id);

/**
 * Reset a suspended remote CPU and resumes its execution.
 * @param cpu_id	ID of the target CPU.
 *
 * Sets the target CPU into the architecture-specific reset set and resumes its
 * execution.
 *
 * @note This function must not be invoked for the caller's CPU or if the
 * target CPU is not in suspend state.
 *
 * @see arch_suspend_cpu
 */
void arch_reset_cpu(unsigned int cpu_id);

/**
 * Park a suspended remote CPU.
 * @param cpu_id	ID of the target CPU.
 *
 * Parking means that the target CPU does not execute cell code but can handle
 * asynchronous events again. Parking is not implemented as busy-waiting and
 * may set the CPU into an appropriate power-saving mode. The CPU can therefore
 * be left in this state for an undefined time.
 *
 * Parking may destroy the cell-visible CPU state and cannot be used to resume
 * cell execution in the previous state without additional measures.
 *
 * @note This function must not be invoked for the caller's CPU or if the
 * target CPU is not in suspend state.
 *
 * @see arch_suspend_cpu
 */
void arch_park_cpu(unsigned int cpu_id);

/**
 * Performs the architecture-specific steps for mapping a memory region into a
 * cell's address space.
 * @param cell		Cell for which the mapping shall be done.
 * @param mem		Memory region to map.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see arch_unmap_memory_region
 */
int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem);

/**
 * Performs the architecture-specific steps for unmapping a memory region from
 * a cell's address space.
 * @param cell		Cell for which the unmapping shall be done.
 * @param mem		Memory region to unmap.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see arch_map_memory_region
 */
int arch_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem);

/**
 * Performs the architecture-specific steps for invalidating memory caches
 * after memory regions have been unmapped from a cell.
 * This function should be called after memory got unmapped or memory access
 * got restricted, and the cell should keep running.
 * @param cell		Cell for which the caches should get flushed
 *
 * @see per_cpu::flush_vcpu_caches
 */
void arch_flush_cell_vcpu_caches(struct cell *cell);

/**
 * Performs the architecture-specific steps for creating a new cell.
 * @param cell		Data structure of the new cell.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see arch_cell_destroy
 */
int arch_cell_create(struct cell *cell);

/**
 * Performs the architecture-specific steps for destroying a cell.
 * @param cell		Cell to be destroyed.
 *
 * @see arch_cell_create
 */
void arch_cell_destroy(struct cell *cell);

/**
 * Performs the architecture-specific steps for resetting a cell.
 * @param cell		Cell to be reset.
 *
 * @note This function shall not reset individual cell CPUs. Instead, this is
 * triggered by the core via arch_reset_cpu().
 *
 * @see arch_reset_cpu
 */
void arch_cell_reset(struct cell *cell);

/**
 * Performs the architecture-specific steps for applying configuration changes.
 * @param cell_added_removed	Cell that was added or removed to/from the
 * 				system or NULL.
 *
 * @see config_commit
 * @see pci_config_commit
 */
void arch_config_commit(struct cell *cell_added_removed);

/**
 * Architecture-specific preparations before shutting down the hypervisor.
 */
void arch_prepare_shutdown(void);

/**
 * Performs the architecture-specifc steps to stop the current CPU on panic.
 *
 * @note This function never returns.
 *
 * @see panic_stop
 */
void __attribute__((noreturn)) arch_panic_stop(void);

/**
 * Performs the architecture-specific steps to park the current CPU on panic.
 *
 * @note This function only marks the CPU as parked and then returns to the
 * caller.
 *
 * @see panic_park
 */
void arch_panic_park(void);

/** @} */
