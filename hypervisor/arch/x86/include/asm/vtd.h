/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/acpi.h>
#include <jailhouse/utils.h>
#include <asm/cell.h>

#include <jailhouse/cell-config.h>

#define ACPI_DMAR_DRHD			0
#define ACPI_DMAR_RMRR			1
#define ACPI_DMAR_ATSR			2
#define ACPI_DMAR_RHSA			3

struct acpi_dmar_remap_header {
	u16 type;
	u16 length;
};

struct acpi_dmar_table {
	struct acpi_table_header header;
	u8 host_address_width;
	u8 flags;
	u8 reserved[10];
	struct acpi_dmar_remap_header remap_structs[];
};

struct acpi_dmar_drhd {
	struct acpi_dmar_remap_header header;
	u8 flags;
	u8 reserved;
	u16 segment;
	u64 register_base_addr;
	u8 device_scope[];
};

#define VTD_ROOT_PRESENT		0x00000001

#define VTD_CTX_PRESENT			0x00000001
#define VTD_CTX_FPD			0x00000002
#define VTD_CTX_TTYPE_MLP_UNTRANS	0x00000000
#define VTD_CTX_TTYPE_MLP_ALL		0x00000004
#define VTD_CTX_TTYPE_PASSTHROUGH	0x00000008

#define VTD_CTX_AGAW_30			0x00000000
#define VTD_CTX_AGAW_39			0x00000001
#define VTD_CTX_AGAW_48			0x00000002
#define VTD_CTX_AGAW_57			0x00000003
#define VTD_CTX_AGAW_64			0x00000004
#define VTD_CTX_DID_SHIFT		8

struct vtd_entry {
	u64 lo_word;
	u64 hi_word;
};

#define VTD_PAGE_READ			0x00000001
#define VTD_PAGE_WRITE			0x00000002

#define VTD_MAX_PAGE_DIR_LEVELS		4

#define VTD_CAP_REG			0x08
# define VTD_CAP_NUM_DID_MASK		BIT_MASK(2, 0)
# define VTD_CAP_CM			(1UL << 7)
# define VTD_CAP_SAGAW39		(1UL << 9)
# define VTD_CAP_SAGAW48		(1UL << 10)
# define VTD_CAP_SLLPS2M		(1UL << 34)
# define VTD_CAP_SLLPS1G		(1UL << 35)
# define VTD_CAP_FRO_MASK		BIT_MASK(33, 24)
#define  VTD_CAP_NFR_MASK		BIT_MASK(47, 40)
#define VTD_ECAP_REG			0x10
# define VTD_ECAP_IRO_MASK		BIT_MASK(17, 8)
#define VTD_GCMD_REG			0x18
# define VTD_GCMD_SRTP			(1UL << 30)
# define VTD_GCMD_TE			(1UL << 31)
#define VTD_GSTS_REG			0x1c
# define VTD_GSTS_RTPS			(1UL << 30)
# define VTD_GSTS_TES			(1UL << 31)
#define VTD_RTADDR_REG			0x20
#define VTD_CCMD_REG			0x28
# define VTD_CCMD_CIRG_GLOBAL		(1UL << 60)
# define VTD_CCMD_CIRG_DOMAIN		(2UL << 60)
# define VTD_CCMD_ICC			(1UL << 63)
#define VTD_FSTS_REG			0x34
# define VTD_FSTS_PFO			(1UL << 0)
# define VTD_FSTS_PFO_CLEAR		1
# define VTD_FSTS_PPF			(1UL << 1)
# define VTD_FSTS_FRI_MASK		BIT_MASK(15, 8)
#define VTD_FECTL_REG			0x38
#define  VTD_FECTL_IM			(1UL << 31)
#define VTD_FEDATA_REG			0x3c
#define VTD_FEADDR_REG			0x40
#define VTD_FEUADDR_REG			0x44
#define VTD_PMEN_REG			0x64
#define VTD_PLMBASE_REG			0x68
#define VTD_PLMLIMIT_REG		0x6c
#define VTD_PHMBASE_REG			0x70
#define VTD_PHMLIMIT_REG		0x78

#define VTD_IOTLB_REG			0x8
# define VTD_IOTLB_DID_SHIFT		32
# define VTD_IOTLB_DW			(1UL << 48)
# define VTD_IOTLB_DR			(1UL << 49)
# define VTD_IOTLB_IIRG_GLOBAL		(1UL << 57)
# define VTD_IOTLB_IIRG_DOMAIN		(2UL << 57)
# define VTD_IOTLB_IVT			(1UL << 63)
# define VTD_IOTLB_R_MASK		BIT_MASK(31, 0)

#define VTD_FRCD_LO_REG			0x0
#define  VTD_FRCD_LO_FI_MASK		BIT_MASK(63, 12)
#define VTD_FRCD_HI_REG			0x8
#define  VTD_FRCD_HI_SID_MASK		BIT_MASK(79-64, 64-64)
#define  VTD_FRCD_HI_FR_MASK		BIT_MASK(103-64, 96-64)
#define  VTD_FRCD_HI_TYPE		(1L << (126-64))
#define  VTD_FRCD_HI_F			(1L << (127-64))
#define  VTD_FRCD_HI_F_CLEAR		1

int vtd_init(void);

int vtd_cell_init(struct cell *cell);
int vtd_map_memory_region(struct cell *cell,
			  const struct jailhouse_memory *mem);
int vtd_unmap_memory_region(struct cell *cell,
			    const struct jailhouse_memory *mem);
void vtd_cell_exit(struct cell *cell);

void vtd_config_commit(struct cell *cell_added_removed);

void vtd_shutdown(void);

void vtd_check_pending_faults(struct per_cpu *cpu_data);
