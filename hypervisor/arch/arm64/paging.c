/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright 2018 NXP
 *
 * Authors:
 *   Peng Fan <peng.fan@nxp.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/percpu.h>
#include <asm/paging.h>

/**
 * Return the physical address bits.
 *
 * In arch_paging_init this value will be kept in cpu_parange
 * for later reference
 *
 * @return The physical address bits.
 */
unsigned int get_cpu_parange(void)
{
	/* Larger than any possible value */
	unsigned int parange = 0x10;
	unsigned int cpu;

	/*
	 * early_init calls paging_init, which will indirectly call
	 * get_cpu_parange, prior to cell_init, we cannot use
	 * for_each_cpu yet. So we need to iterate over the configuration
	 * of the root cell.
	 */
	for (cpu = 0; cpu < system_config->root_cell.cpu_set_size * 8; cpu++)
		if (cpu_id_valid(cpu) &&
		    (per_cpu(cpu)->id_aa64mmfr0 & 0xf) < parange)
			parange = per_cpu(cpu)->id_aa64mmfr0 & 0xf;

	switch (parange) {
	case PARANGE_32B:
		return 32;
	case PARANGE_36B:
		return 36;
	case PARANGE_40B:
		return 40;
	case PARANGE_42B:
		return 42;
	case PARANGE_44B:
		return 44;
	case PARANGE_48B:
		return 48;
	default:
		return 0;
	}
}
