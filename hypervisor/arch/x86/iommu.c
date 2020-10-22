/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2020
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
	       system_config->platform_info.iommu_units[units].base)
		units++;
	return units;
}

struct public_per_cpu *iommu_select_fault_reporting_cpu(void)
{
	/*
	 * The selection process assumes that at least one bit is set somewhere
	 * because we don't support configurations where Linux is left with no
	 * CPUs.
	 * Save this value globally to avoid multiple reports of the same
	 * case from different CPUs.
	 */
	fault_reporting_cpu_id = first_cpu(root_cell.cpu_set);

	return public_per_cpu(fault_reporting_cpu_id);
}
