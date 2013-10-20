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

#ifndef _JAILHOUSE_ENTRY_H
#define _JAILHOUSE_ENTRY_H

#include <jailhouse/header.h>
#include <asm/percpu.h>

#include <jailhouse/cell-config.h>

#define EIO		5
#define ENOMEM		12
#define EBUSY		16
#define ENODEV		19
#define EINVAL		22
#define ERANGE		34
#define ENOSYS		38

extern struct jailhouse_header hypervisor_header;
extern void *config_memory;

int arch_entry(int cpu_id);
void got_init(void);
void vm_exit(void);

int entry(struct per_cpu *cpu_data);

int arch_init_early(struct cell *linux_cell,
		    struct jailhouse_cell_desc *config);
int arch_cpu_init(struct per_cpu *cpu_data);
int arch_init_late(struct cell *linux_cell,
		   struct jailhouse_cell_desc *config);
void __attribute__((noreturn)) arch_cpu_activate_vmm(struct per_cpu *cpu_data);
void arch_cpu_restore(struct per_cpu *cpu_data);

#endif /* !_JAILHOUSE_ENTRY_H */
