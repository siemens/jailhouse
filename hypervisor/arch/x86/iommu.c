/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <asm/iommu.h>

unsigned int fault_reporting_cpu_id;

unsigned int iommu_count_units(void)
{
	unsigned int units = 0;

	while (units < JAILHOUSE_MAX_IOMMU_UNITS &&
	       system_config->platform_info.x86.iommu_units[units].base)
		units++;
	return units;
}

struct public_per_cpu *iommu_select_fault_reporting_cpu(void)
{
	struct public_per_cpu *target_data;
	unsigned int n;

	/* This assumes that at least one bit is set somewhere because we
	 * don't support configurations where Linux is left with no CPUs. */
	for (n = 0; root_cell.cpu_set->bitmap[n] == 0; n++)
		/* Empty loop */;
	target_data = public_per_cpu(ffsl(root_cell.cpu_set->bitmap[n]));

	/* Save this value globally to avoid multiple reports of the same
	 * case from different CPUs */
	fault_reporting_cpu_id = target_data->cpu_id;

	return target_data;
}
