/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/cell.h>
#include <asm/psci.h>
#include <asm/smp.h>

static struct smp_ops sun7i_smp_ops = {
	.init = psci_cell_init,
	.cpu_spin = psci_emulate_spin,
};

void register_smp_ops(struct cell *cell)
{
	cell->arch.smp = &sun7i_smp_ops;
}
