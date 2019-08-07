/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2018 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors:
 *  Nikhil Devshatwar <nikhil.nd@ti.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <asm/iommu.h>

unsigned int iommu_count_units(void)
{
	unsigned int units = 0;

	while (units < JAILHOUSE_MAX_IOMMU_UNITS &&
	       system_config->platform_info.arm.iommu_units[units].base)
		units++;
	return units;
}

int iommu_map_memory_region(struct cell *cell,
			    const struct jailhouse_memory *mem)
{
	return 0;
}

int iommu_unmap_memory_region(struct cell *cell,
			      const struct jailhouse_memory *mem)
{
	return 0;
}

void iommu_config_commit(struct cell *cell)
{
}
