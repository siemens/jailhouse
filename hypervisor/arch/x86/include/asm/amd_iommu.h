/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Valentine Sinitsyn, 2014, 2015
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_AMD_IOMMU_H
#define _JAILHOUSE_ASM_AMD_IOMMU_H

#include <jailhouse/types.h>
#include <jailhouse/utils.h>

#include <jailhouse/cell-config.h>

#define AMD_IOMMU_PTE_P			(1ULL <<  0)
#define AMD_IOMMU_PTE_PG_MODE(level)	((level) << 9)
#define AMD_IOMMU_PTE_PG_MODE_MASK	BIT_MASK(11, 9)
#define AMD_IOMMU_PTE_IR		(1ULL << 61)
#define AMD_IOMMU_PTE_IW		(1ULL << 62)

#define AMD_IOMMU_PAGE_DEFAULT_FLAGS	(AMD_IOMMU_PTE_IW | AMD_IOMMU_PTE_IR | \
					 AMD_IOMMU_PTE_P)

u64 amd_iommu_get_memory_region_flags(const struct jailhouse_memory *mem);

#endif
