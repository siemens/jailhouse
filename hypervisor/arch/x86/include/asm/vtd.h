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
#include <asm/types.h>

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
#define VTD_CTX_DID16_MASK		0x00000f00

struct vtd_entry {
	u64 lo_word;
	u64 hi_word;
};

#define VTD_PAGE_READ			0x00000001
#define VTD_PAGE_WRITE			0x00000002

#define VTD_CAP_REG			0x08
# define VTD_CAP_SAGAW30		0x00000100
# define VTD_CAP_SAGAW39		0x00000200
# define VTD_CAP_SAGAW48		0x00000400
# define VTD_CAP_SAGAW57		0x00000800
# define VTD_CAP_SAGAW64		0x00001000
#define VTD_ECAP_REG			0x10
#define VTD_GCMD_REG			0x18
# define VTD_GCMD_SRTP			0x40000000
# define VTD_GCMD_TE			0x80000000
#define VTD_GSTS_REG			0x1C
# define VTD_GSTS_SRTP			0x40000000
# define VTD_GSTS_TE			0x80000000
#define VTD_RTADDR_REG			0x20
#define VTD_CCMD_REG			0x28
#define VTD_PMEN_REG			0x64
#define VTD_PLMBASE_REG			0x68
#define VTD_PLMLIMIT_REG		0x6C
#define VTD_PHMBASE_REG			0x70
#define VTD_PHMLIMIT_REG		0x78

int vtd_init(void);
int vtd_cell_init(struct cell *cell, struct jailhouse_cell_desc *config);
void vtd_shutdown(void);
