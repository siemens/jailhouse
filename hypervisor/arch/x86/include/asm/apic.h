/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_APIC_H
#define _JAILHOUSE_ASM_APIC_H

#include <jailhouse/paging.h>
#include <jailhouse/utils.h>
#include <jailhouse/percpu.h>

/* currently our limit due to fixed-size APID ID map */
#define APIC_MAX_PHYS_ID		254
#define CPU_ID_INVALID			255

#define XAPIC_BASE			0xfee00000

#define APIC_BASE_EXTD			(1 << 10)
#define APIC_BASE_EN			(1 << 11)

#define APIC_NUM_INT_REGS		(256/32)

#define APIC_REG_ID			0x02
#define APIC_REG_LVR			0x03
#define APIC_REG_TPR			0x08
#define APIC_REG_EOI			0x0b
#define APIC_REG_LDR			0x0d
#define APIC_REG_DFR			0x0e
#define APIC_REG_SVR			0x0f
#define APIC_REG_ISR0			0x10
#define APIC_REG_LVTCMCI		0x2f
#define APIC_REG_ICR			0x30
#define APIC_REG_ICR_HI			0x31
#define APIC_REG_LVTT			0x32
#define APIC_REG_LVTTHMR		0x33
#define APIC_REG_LVTPC			0x34
#define APIC_REG_LVT0			0x35
#define APIC_REG_LVT1			0x36
#define APIC_REG_LVTERR			0x37
#define APIC_REG_SELF_IPI		0x3f
#define APIC_REG_XFEAT			0x40
#define APIC_REG_XLVT0			0x50
#define APIC_REG_XLVT3			0x53

#define APIC_EOI_ACK			0
#define APIC_SVR_ENABLE_APIC		(1 << 8)
#define APIC_ICR_VECTOR_MASK		BIT_MASK(7, 0)
#define APIC_ICR_DLVR_MASK		BIT_MASK(10, 8)
#define APIC_ICR_DLVR_SHIFT		8
#define  APIC_ICR_DLVR_FIXED		(0b000 << APIC_ICR_DLVR_SHIFT)
#define  APIC_ICR_DLVR_LOWPRI		(0b001 << APIC_ICR_DLVR_SHIFT)
#define  APIC_ICR_DLVR_SMI		(0b010 << APIC_ICR_DLVR_SHIFT)
#define  APIC_ICR_DLVR_NMI		(0b100 << APIC_ICR_DLVR_SHIFT)
#define  APIC_ICR_DLVR_INIT		(0b101 << APIC_ICR_DLVR_SHIFT)
#define  APIC_ICR_DLVR_SIPI		(0b110 << APIC_ICR_DLVR_SHIFT)
#define APIC_ICR_DEST_PHYSICAL		(0 << 11)
#define APIC_ICR_DEST_LOGICAL		(1 << 11)
#define APIC_ICR_DS_PENDING		(1 << 12)
#define APIC_ICR_LVTM_MASK		BIT_MASK(15, 14)
#define  APIC_ICR_LV_DEASSERT		(0 << 14)
#define  APIC_ICR_LV_ASSERT		(1 << 14)
#define  APIC_ICR_TM_EDGE		(0 << 15)
#define  APIC_ICR_TM_LEVEL		(1 << 15)
#define APIC_ICR_SH_MASK		BIT_MASK(19, 18)
#define  APIC_ICR_SH_NONE		(0b00 << 18)
#define  APIC_ICR_SH_SELF		(0b01 << 18)
#define  APIC_ICR_SH_ALL		(0b10 << 18)
#define  APIC_ICR_SH_ALLOTHER		(0b11 << 18)

#define APIC_LVT_DLVR_MASK		BIT_MASK(10, 8)
#define  APIC_LVT_DLVR_FIXED		(0b000 << 8)
#define  APIC_LVT_DLVR_NMI		(0b100 << 8)
#define APIC_LVT_MASKED			(1 << 16)

#define APIC_LVR_EAS			(1 << 31)

#define XAPIC_DEST_MASK			BIT_MASK(31, 24)
#define XAPIC_DEST_SHIFT		24

#define X2APIC_DEST_LOGICAL_ID_MASK	BIT_MASK(15, 0)
#define X2APIC_DEST_CLUSTER_ID_MASK	BIT_MASK(31, 16)
#define X2APIC_DEST_CLUSTER_ID_SHIFT	16

#define X2APIC_CLUSTER_ID_SHIFT		4

#define APIC_BSP_PSEUDO_SIPI		0x100

/**
 * @ingroup X86
 * @defgroup X86-APIC APIC
 *
 * The x86 Advanced Programmable Interrupt Controller supports xAPIC and x2APIC.
 *
 * @{
 */

union x86_msi_vector {
	struct {
		u64 unused:2,
		    dest_logical:1,
		    redir_hint:1,
		    reserved1:8,
		    destination:8,
		    address:44;
		u32 vector:8,
		    delivery_mode:3,
		    reserved:21;
	} __attribute__((packed)) native;
	struct {
		u64 unused:2,
		    int_index15:1,
		    shv:1,
		    remapped:1,
		    int_index:15,
		    address:44;
		u16 subhandle;
		u16 zero;
	} __attribute__((packed)) remap;
	struct {
		u64 address;
		u32 data;
	} __attribute__((packed)) raw;
} __attribute__((packed));

/* MSI delivery modes */
#define MSI_DM_NMI			(0x4 << 8)

#define MSI_ADDRESS_VALUE		0xfee

/* APIC IRQ message: delivery modes */
#define APIC_MSG_DLVR_FIXED		0x0
#define APIC_MSG_DLVR_LOWPRI		0x1

struct apic_irq_message {
	u8 vector;
	u8 delivery_mode:3;
	u8 dest_logical:1;
	u8 level_triggered:1;
	u8 redir_hint:1;
	u8 valid:1;
	u32 destination;
};

extern bool using_x2apic;

int apic_init(void);
int apic_cpu_init(struct per_cpu *cpu_data);

void apic_clear(void);

void apic_send_nmi_ipi(struct public_per_cpu *target_data);
bool apic_filter_irq_dest(struct cell *cell, struct apic_irq_message *irq_msg);
void apic_send_irq(struct apic_irq_message irq_msg);

void apic_irq_handler(void);

unsigned int apic_mmio_access(const struct guest_paging_structures *pg_structs,
			      unsigned int reg, bool is_write);

bool x2apic_handle_write(void);
void x2apic_handle_read(void);

u32 x2apic_filter_logical_dest(struct cell *cell, u32 destination);

/** @} */
#endif /* !_JAILHOUSE_ASM_APIC_H */
