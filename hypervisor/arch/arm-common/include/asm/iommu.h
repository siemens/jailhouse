/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2019 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors:
 *  Nikhil Devshatwar <nikhil.nd@ti.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_IOMMU_H
#define _JAILHOUSE_ASM_IOMMU_H

#include <jailhouse/cell.h>
#include <jailhouse/utils.h>
#include <jailhouse/cell-config.h>

#define for_each_stream_id(sid, config, counter)			       \
	for ((sid) = (jailhouse_cell_stream_ids(config)[0]), (counter) = 0;    \
	     (counter) < (config)->num_stream_ids;			       \
	     (sid) = (jailhouse_cell_stream_ids(config)[++(counter)]))

unsigned int iommu_count_units(void);
int iommu_map_memory_region(struct cell *cell,
			    const struct jailhouse_memory *mem);
int iommu_unmap_memory_region(struct cell *cell,
			      const struct jailhouse_memory *mem);
void iommu_config_commit(struct cell *cell);
#endif
