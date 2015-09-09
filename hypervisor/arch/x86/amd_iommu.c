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

#include <jailhouse/cell.h>
#include <jailhouse/cell-config.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <asm/amd_iommu.h>
#include <asm/apic.h>
#include <asm/iommu.h>

#define CAPS_IOMMU_HEADER_REG		0x00
#define  CAPS_IOMMU_EFR_SUP		(1 << 27)
#define CAPS_IOMMU_BASE_LOW_REG		0x04
#define  CAPS_IOMMU_ENABLE		(1 << 0)
#define CAPS_IOMMU_BASE_HI_REG		0x08

#define ACPI_REPORTING_HE_SUP		(1 << 7)

#define AMD_DEV_TABLE_BASE_REG		0x0000
#define AMD_CMD_BUF_BASE_REG		0x0008
#define AMD_EVT_LOG_BASE_REG		0x0010
#define AMD_CONTROL_REG			0x0018
#define  AMD_CONTROL_IOMMU_EN		(1UL << 0)
#define  AMD_CONTROL_EVT_LOG_EN		(1UL << 2)
#define  AMD_CONTROL_EVT_INT_EN		(1UL << 3)
#define  AMD_CONTROL_COMM_WAIT_INT_EN	(1UL << 4)
#define  AMD_CONTROL_CMD_BUF_EN		(1UL << 12)
#define  AMD_CONTROL_SMIF_EN		(1UL << 22)
#define  AMD_CONTROL_SMIFLOG_EN		(1UL << 24)
#define  AMD_CONTROL_SEG_EN_MASK	BIT_MASK(36, 34)
#define  AMD_CONTROL_SEG_EN_SHIFT	34
#define AMD_EXT_FEATURES_REG		0x0030
#define  AMD_EXT_FEAT_HE_SUP		(1UL << 7)
#define  AMD_EXT_FEAT_SMI_FSUP_MASK	BIT_MASK(17, 16)
#define  AMD_EXT_FEAT_SMI_FSUP_SHIFT	16
#define  AMD_EXT_FEAT_SMI_FRC_MASK	BIT_MASK(20, 18)
#define  AMD_EXT_FEAT_SMI_FRC_SHIFT	18
#define  AMD_EXT_FEAT_SEG_SUP_MASK	BIT_MASK(39, 38)
#define  AMD_EXT_FEAT_SEG_SUP_SHIFT   	38
#define AMD_HEV_UPPER_REG		0x0040
#define AMD_HEV_LOWER_REG		0x0048
#define AMD_HEV_STATUS_REG		0x0050
#define  AMD_HEV_VALID			(1UL << 1)
#define  AMD_HEV_OVERFLOW		(1UL << 2)
#define AMD_SMI_FILTER0_REG		0x0060
#define  AMD_SMI_FILTER_VALID		(1UL << 16)
#define  AMD_SMI_FILTER_LOCKED		(1UL << 17)
#define AMD_DEV_TABLE_SEG1_REG		0x0100
#define AMD_CMD_BUF_HEAD_REG		0x2000
#define AMD_CMD_BUF_TAIL_REG		0x2008
#define AMD_EVT_LOG_HEAD_REG		0x2010
#define AMD_EVT_LOG_TAIL_REG		0x2018
#define AMD_STATUS_REG			0x2020
# define AMD_STATUS_EVT_OVERFLOW	(1UL << 0)
# define AMD_STATUS_EVT_LOG_INT		(1UL << 1)
# define AMD_STATUS_EVT_LOG_RUN		(1UL << 3)

struct dev_table_entry {
	u64 raw64[4];
} __attribute__((packed));

#define DTE_VALID			(1UL << 0)
#define DTE_TRANSLATION_VALID		(1UL << 1)
#define DTE_PAGING_MODE_4_LEVEL		(4UL << 9)
#define DTE_IR				(1UL << 61)
#define DTE_IW				(1UL << 62)

#define DEV_TABLE_SEG_MAX		8
#define DEV_TABLE_SIZE			0x200000

union buf_entry {
	u32 raw32[4];
	u64 raw64[2];
	struct {
		u32 pad0;
		u32 pad1:28;
		u32 type:4;
	};
} __attribute__((packed));

#define CMD_COMPL_WAIT			0x01
# define CMD_COMPL_WAIT_STORE		(1 << 0)
# define CMD_COMPL_WAIT_INT		(1 << 1)

#define CMD_INV_DEVTAB_ENTRY		0x02

#define CMD_INV_IOMMU_PAGES		0x03
# define CMD_INV_IOMMU_PAGES_SIZE	(1 << 0)
# define CMD_INV_IOMMU_PAGES_PDE	(1 << 1)

#define EVENT_TYPE_ILL_DEV_TAB_ENTRY	0x01
#define EVENT_TYPE_PAGE_TAB_HW_ERR	0x04
#define EVENT_TYPE_ILL_CMD_ERR		0x05
#define EVENT_TYPE_CMD_HW_ERR		0x06
#define EVENT_TYPE_IOTLB_INV_TIMEOUT	0x07
#define EVENT_TYPE_INV_PPR_REQ		0x09

#define BUF_LEN_EXPONENT_SHIFT		56

unsigned int iommu_mmio_count_regions(struct cell *cell)
{
	return 0;
}

int iommu_init(void)
{
	printk("WARNING: AMD IOMMU support is not implemented yet\n");
	/* TODO: Implement */
	return 0;
}

int iommu_cell_init(struct cell *cell)
{
	/* TODO: Implement */
	return 0;
}

int iommu_map_memory_region(struct cell *cell,
			    const struct jailhouse_memory *mem)
{
	/* TODO: Implement */
	return 0;
}
int iommu_unmap_memory_region(struct cell *cell,
			      const struct jailhouse_memory *mem)
{
	/* TODO: Implement */
	return 0;
}

int iommu_add_pci_device(struct cell *cell, struct pci_device *device)
{
	/* TODO: Implement */
	return 0;
}

void iommu_remove_pci_device(struct pci_device *device)
{
	/* TODO: Implement */
}

void iommu_cell_exit(struct cell *cell)
{
	/* TODO: Implement */
}

void iommu_config_commit(struct cell *cell_added_removed)
{
	/* TODO: Implement */
}

struct apic_irq_message iommu_get_remapped_root_int(unsigned int iommu,
						    u16 device_id,
						    unsigned int vector,
						    unsigned int remap_index)
{
	struct apic_irq_message dummy = { .valid = 0 };

	/* TODO: Implement */
	return dummy;
}

int iommu_map_interrupt(struct cell *cell, u16 device_id, unsigned int vector,
			struct apic_irq_message irq_msg)
{
	/* TODO: Implement */
	return -ENOSYS;
}

void iommu_shutdown(void)
{
	/* TODO: Implement */
}

void iommu_check_pending_faults(void)
{
	/* TODO: Implement */
}

bool iommu_cell_emulates_ir(struct cell *cell)
{
	/* TODO: Implement */
	return false;
}
