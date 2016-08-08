/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef JAILHOUSE_ASM_SMP_H_
#define JAILHOUSE_ASM_SMP_H_

#ifndef __ASSEMBLY__

struct mmio_access;
struct per_cpu;
struct cell;

struct smp_ops {
	/* Returns an address */
	unsigned long (*cpu_spin)(struct per_cpu *cpu_data);
};

extern const unsigned int smp_mmio_regions;

unsigned long arch_smp_spin(struct per_cpu *cpu_data, struct smp_ops *ops);
void register_smp_ops(struct cell *cell);

int smp_init(void);

void smp_cell_init(struct cell *cell);
void smp_cell_exit(struct cell *cell);

#endif /* !__ASSEMBLY__ */
#endif /* !JAILHOUSE_ASM_SMP_H_ */
