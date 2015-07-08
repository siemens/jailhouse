/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_VTD_H
#define _JAILHOUSE_ASM_VTD_H

#include <jailhouse/cell.h>
#include <jailhouse/pci.h>
#include <jailhouse/utils.h>
#include <asm/apic.h>

#include <jailhouse/cell-config.h>

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

#define VTD_MAX_PAGE_TABLE_LEVELS	4

#define VTD_VER_REG			0x00
# define VTD_VER_MASK			BIT_MASK(7, 0)
# define VTD_VER_MIN			0x10
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
# define VTD_ECAP_QI			(1UL << 1)
# define VTD_ECAP_IR			(1UL << 3)
# define VTD_ECAP_EIM			(1UL << 4)
#define VTD_GCMD_REG			0x18
# define VTD_GCMD_SIRTP			(1UL << 24)
# define VTD_GCMD_IRE			(1UL << 25)
# define VTD_GCMD_QIE			(1UL << 26)
# define VTD_GCMD_SRTP			(1UL << 30)
# define VTD_GCMD_TE			(1UL << 31)
#define VTD_GSTS_REG			0x1c
# define VTD_GSTS_IRES			(1UL << 25)
# define VTD_GSTS_QIES			(1UL << 26)
# define VTD_GSTS_TES			(1UL << 31)
# define VTD_GSTS_USED_CTRLS \
	(VTD_GSTS_IRES | VTD_GSTS_QIES | VTD_GSTS_TES)
#define VTD_RTADDR_REG			0x20
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
#define VTD_IQH_REG			0x80
# define VTD_IQH_QH_SHIFT		4
#define VTD_IQT_REG			0x88
# define VTD_IQT_QT_MASK		BIT_MASK(18, 4)
# define VTD_IQT_QT_SHIFT		4
#define VTD_IQA_REG			0x90
# define VTD_IQA_ADDR_MASK		BIT_MASK(63, 12)
#define VTD_IRTA_REG			0xb8
# define VTD_IRTA_SIZE_MASK		BIT_MASK(3, 0)
# define VTD_IRTA_EIME			(1UL << 11)
# define VTD_IRTA_ADDR_MASK		BIT_MASK(63, 12)

#define VTD_REQ_INV_MASK		BIT_MASK(3, 0)

#define VTD_REQ_INV_CONTEXT		0x01
# define VTD_INV_CONTEXT_GLOBAL		(1UL << 4)
# define VTD_INV_CONTEXT_DOMAIN		(2UL << 4)
# define VTD_INV_CONTEXT_DOMAIN_SHIFT	16

#define VTD_REQ_INV_IOTLB		0x02
# define VTD_INV_IOTLB_GLOBAL		(1UL << 4)
# define VTD_INV_IOTLB_DOMAIN		(2UL << 4)
# define VTD_INV_IOTLB_DW		(1UL << 6)
# define VTD_INV_IOTLB_DR		(1UL << 7)
# define VTD_INV_IOTLB_DOMAIN_SHIFT	16

#define VTD_REQ_INV_INT			0x04
# define VTD_INV_INT_GLOBAL		(0UL << 4)
# define VTD_INV_INT_INDEX		(1UL << 4)
# define VTD_INV_INT_IM_MASK		BIT_MASK(31, 27)
# define VTD_INV_INT_IM_SHIFT		27
# define VTD_INV_INT_IIDX_MASK		BIT_MASK(47, 32)
# define VTD_INV_INT_IIDX_SHIFT		32

#define VTD_REQ_INV_WAIT		0x05
#define  VTD_INV_WAIT_IF		(1UL << 4)
#define  VTD_INV_WAIT_SW		(1UL << 5)
#define  VTD_INV_WAIT_FN		(1UL << 6)
#define  VTD_INV_WAIT_SDATA_SHIFT	32

#define VTD_FRCD_LO_REG			0x0
#define  VTD_FRCD_LO_FI_MASK		BIT_MASK(63, 12)
#define VTD_FRCD_HI_REG			0x8
#define  VTD_FRCD_HI_SID_MASK		BIT_MASK(79-64, 64-64)
#define  VTD_FRCD_HI_FR_MASK		BIT_MASK(103-64, 96-64)
#define  VTD_FRCD_HI_TYPE		(1L << (126-64))
#define  VTD_FRCD_HI_F			(1L << (127-64))
#define  VTD_FRCD_HI_F_CLEAR		1

union vtd_irte {
	struct {
		u8 p:1;
		u8 fpd:1;
		u8 dest_logical:1;
		u8 redir_hint:1;
		u8 level_triggered:1;
		u8 delivery_mode:3;
		u8 assigned:1;
		u8 reserved:7;
		u8 vector;
		u8 reserved2;
		u32 destination;
		u16 sid;
		u16 sq:2;
		u16 svt:2;
		u16 reserved3:12;
		u32 reserved4;
	} __attribute__((packed)) field;
	u64 raw[2];
} __attribute__((packed));

#define VTD_IRTE_SQ_VERIFY_FULL_SID	0x0
#define VTD_IRTE_SVT_VERIFY_SID_SQ	0x1

#endif
