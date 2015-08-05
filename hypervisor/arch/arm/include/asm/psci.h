/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_PSCI_H
#define _JAILHOUSE_ASM_PSCI_H

#define PSCI_VERSION			0x84000000
#define PSCI_CPU_SUSPEND_32		0x84000001
#define PSCI_CPU_SUSPEND_64		0xc4000001
#define PSCI_CPU_OFF			0x84000002
#define PSCI_CPU_ON_32			0x84000003
#define PSCI_CPU_ON_64			0xc4000003
#define PSCI_AFFINITY_INFO_32		0x84000004
#define PSCI_AFFINITY_INFO_64		0xc4000004
#define PSCI_MIGRATE_32			0x84000005
#define PSCI_MIGRATE_64			0xc4000005
#define PSCI_MIGRATE_INFO_TYPE		0x84000006
#define PSCI_MIGRATE_INFO_UP_CPU_32	0x84000007
#define PSCI_MIGRATE_INFO_UP_CPU_64	0xc4000007
#define PSCI_SYSTEM_OFF			0x84000008
#define PSCI_SYSTEM_RESET		0x84000009

/* v0.1 function IDs as used by U-Boot */
#define PSCI_CPU_OFF_V0_1_UBOOT		0x95c1ba5f
#define PSCI_CPU_ON_V0_1_UBOOT		0x95c1ba60

#define PSCI_SUCCESS		0
#define PSCI_NOT_SUPPORTED	(-1)
#define PSCI_INVALID_PARAMETERS	(-2)
#define PSCI_DENIED		(-3)
#define PSCI_ALREADY_ON		(-4)
#define PSCI_ON_PENDING		(-5)
#define PSCI_INTERNAL_FAILURE	(-6)
#define PSCI_NOT_PRESENT	(-7)
#define PSCI_DISABLED		(-8)

#define PSCI_CPU_IS_ON		0
#define PSCI_CPU_IS_OFF		1

#define IS_PSCI_FN(hvc)		((((hvc) >> 24) & 0x84) == 0x84)

#define PSCI_INVALID_ADDRESS	0xffffffff

#ifndef __ASSEMBLY__

#include <jailhouse/types.h>

struct cell;
struct trap_context;
struct per_cpu;

struct psci_mbox {
	unsigned long entry;
	unsigned long context;
};

void psci_cpu_off(struct per_cpu *cpu_data);
long psci_cpu_on(unsigned int target, unsigned long entry,
		 unsigned long context);
bool psci_cpu_stopped(unsigned int cpu_id);
int psci_wait_cpu_stopped(unsigned int cpu_id);

void psci_suspend(struct per_cpu *cpu_data);
long psci_resume(unsigned int target);
long psci_try_resume(unsigned int cpu_id);

long psci_dispatch(struct trap_context *ctx);

int psci_cell_init(struct cell *cell);
unsigned long psci_emulate_spin(struct per_cpu *cpu_data);

#endif /* !__ASSEMBLY__ */
#endif /* _JAILHOUSE_ASM_PSCI_H */
