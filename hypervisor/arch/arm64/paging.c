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

unsigned int cpu_parange_encoded;

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
	static const unsigned int pa_bits[] = { 32, 36, 40, 42, 44, 48 };
	unsigned int cpu;

	/* Largest supported value (for 4K paging) */
	cpu_parange_encoded = PARANGE_48B;

	/*
	 * early_init calls paging_init, which will indirectly call
	 * get_cpu_parange, prior to cell_init, we cannot use
	 * for_each_cpu yet. So we need to iterate over the configuration
	 * of the root cell.
	 */
	for (cpu = 0; cpu < system_config->root_cell.cpu_set_size * 8; cpu++)
		if (cpu_id_valid(cpu) &&
		    (per_cpu(cpu)->id_aa64mmfr0 & 0xf) < cpu_parange_encoded)
			cpu_parange_encoded = per_cpu(cpu)->id_aa64mmfr0 & 0xf;

	return cpu_parange_encoded < ARRAY_SIZE(pa_bits) ?
		pa_bits[cpu_parange_encoded] : 0;
}
