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

#include <asm/types.h>
#include <asm/percpu.h>
#include <jailhouse/cell-config.h>

#define SHUTDOWN_NONE			0
#define SHUTDOWN_STARTED		1

extern struct jailhouse_system *system_config;

unsigned int next_cpu(unsigned int cpu, struct cpu_set *cpu_set,
		      int exception);

#define for_each_cpu(cpu, set)					\
	for ((cpu) = -1;					\
	     (cpu) = next_cpu((cpu), (set), -1),		\
	     (cpu) <= (set)->max_cpu_id;			\
	    )

#define for_each_cpu_except(cpu, set, exception)		\
	for ((cpu) = -1;					\
	     (cpu) = next_cpu((cpu), (set), (exception)),	\
	     (cpu) <= (set)->max_cpu_id;			\
	    )

int check_mem_regions(const struct jailhouse_cell_desc *config);
int cell_init(struct cell *cell, bool copy_cpu_set);

int cell_create(struct per_cpu *cpu_data, unsigned long config_address);
int cell_destroy(struct per_cpu *cpu_data, unsigned long id);

int shutdown(struct per_cpu *cpu_data);

void arch_suspend_cpu(unsigned int cpu_id);
void arch_resume_cpu(unsigned int cpu_id);
void arch_reset_cpu(unsigned int cpu_id);
void arch_park_cpu(unsigned int cpu_id);
void arch_shutdown_cpu(unsigned int cpu_id);

int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem);
void arch_unmap_memory_region(struct cell *cell,
			      const struct jailhouse_memory *mem);

int arch_cell_create(struct per_cpu *cpu_data, struct cell *cell);
void arch_cell_destroy(struct per_cpu *cpu_data, struct cell *cell);

void arch_shutdown(void);
