/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2018 Texas Instruments Incorporated - http://www.ti.com/
 *
 * TI PVU IOMMU unit API headers
 *
 * Authors:
 *  Nikhil Devshatwar <nikhil.nd@ti.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _IOMMMU_PVU_H_
#define _IOMMMU_PVU_H_

#include <jailhouse/cell.h>
#include <jailhouse/cell-config.h>

#define PVU_NUM_TLBS			64
#define PVU_NUM_ENTRIES			8

#define PVU_CONFIG_NTLB_MASK		(0xff)
#define PVU_CONFIG_NENT_MASK		(0xf << 16)

#define PVU_MAX_VIRTID_MASK		(0xfff)

#define PVU_ENABLE_DIS			(0x0)
#define PVU_ENABLE_EN			(0x1)
#define PVU_ENABLE_MASK			(0x1)

struct pvu_hw_cfg {
	u32		pid;
	u32		config;
	u8		resv_16[8];
	u32		enable;
	u32		virtid_map1;
	u32		virtid_map2;
	u8		resv_48[20];
	u32		exception_logging_disable;
	u8		resv_260[208];
	u32		destination_id;
	u8		resv_288[24];
	u32		exception_logging_control;
	u32		exception_logging_header0;
	u32		exception_logging_header1;
	u32		exception_logging_data0;
	u32		exception_logging_data1;
	u32		exception_logging_data2;
	u32		exception_logging_data3;
	u8		resv_320[4];
	u32		exception_pend_set;
	u32		exception_pend_clear;
	u32		exception_ENABLE_set;
	u32		exception_ENABLE_clear;
	u32		eoi_reg;
};

#define PVU_TLB_ENTRY_VALID		(2)
#define PVU_TLB_ENTRY_INVALID		(0)
#define PVU_TLB_ENTRY_MODE_MASK		(0x3 << 30)
#define PVU_TLB_ENTRY_FLAG_MASK		(0xff7f)
#define PVU_TLB_ENTRY_PGSIZE_MASK	(0xf << 16)

#define PVU_ENTRY_INVALID		(0 << 30)
#define PVU_ENTRY_VALID			(2 << 30)

#define LPAE_PAGE_PERM_UR		(1 << 15)
#define LPAE_PAGE_PERM_UW		(1 << 14)
#define LPAE_PAGE_PERM_UX		(1 << 13)
#define LPAE_PAGE_PERM_SR		(1 << 12)
#define LPAE_PAGE_PERM_SW		(1 << 11)
#define LPAE_PAGE_PERM_SX		(1 << 10)

#define LPAE_PAGE_MEM_WRITETHROUGH	(2 << 8)
#define LPAE_PAGE_OUTER_SHARABLE	(1 << 4)
#define LPAE_PAGE_IS_NOALLOC		(0 << 2)
#define LPAE_PAGE_OS_NOALLOC		(0 << 0)

struct pvu_hw_tlb_entry {
	u32		reg0;
	u32		reg1;
	u32		reg2;
	u32		reg3;
	u32		reg4;
	u32		reg5;
	u32		reg6;
	u32		reg7;
};

#define PVU_TLB_EN_MASK			(1 << 31)
#define PVU_TLB_LOG_DIS_MASK		(1 << 30)
#define PVU_TLB_FAULT_MASK		(1 << 29)
#define PVU_TLB_CHAIN_MASK		(0xfff)

struct pvu_hw_tlb {
	u32			chain;
	u8			resv_32[28];
	struct pvu_hw_tlb_entry	entry[8];
	u8			resv_4096[3808];
};

struct pvu_tlb_entry {
	u64		virt_addr;
	u64		phys_addr;
	u64		size;
	u64		flags;
};

struct pvu_dev {
	u32		*cfg_base;
	u32		*tlb_base;

	u32		num_tlbs;
	u32		num_entries;
	u16		max_virtid;

	u16		tlb_data[PVU_NUM_TLBS];
	u16		free_tlb_count;
};

int pvu_iommu_map_memory(struct cell *cell,
		const struct jailhouse_memory *mem);

int pvu_iommu_unmap_memory(struct cell *cell,
		const struct jailhouse_memory *mem);

void pvu_iommu_config_commit(struct cell *cell);

#endif /* _IOMMMU_PVU_H_ */
