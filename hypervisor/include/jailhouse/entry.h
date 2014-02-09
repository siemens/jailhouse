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
#include <asm/types.h>

#include <jailhouse/cell-config.h>

#define EPERM		1
#define ENOENT		2
#define EIO		5
#define E2BIG		7
#define ENOMEM		12
#define EBUSY		16
#define EEXIST		17
#define ENODEV		19
#define EINVAL		22
#define ERANGE		34
#define ENOSYS		38

struct per_cpu;
struct cell;

extern struct jailhouse_header hypervisor_header;
extern void *config_memory;

int arch_entry(unsigned int cpu_id);
void vm_exit(void);

int entry(unsigned int cpu_id, struct per_cpu *cpu_data);

int arch_init_early(struct cell *linux_cell);
int arch_cpu_init(struct per_cpu *cpu_data);
int arch_init_late(struct cell *linux_cell);
void __attribute__((noreturn)) arch_cpu_activate_vmm(struct per_cpu *cpu_data);
void arch_cpu_restore(struct per_cpu *cpu_data);

#endif /* !_JAILHOUSE_ENTRY_H */
