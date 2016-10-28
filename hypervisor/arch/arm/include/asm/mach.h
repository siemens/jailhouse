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

struct cell;

extern const unsigned int mach_mmio_regions;

int mach_init(void);

void mach_cell_init(struct cell *cell);
void mach_cell_exit(struct cell *cell);

#endif /* !JAILHOUSE_ASM_SMP_H_ */
