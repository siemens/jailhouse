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

#include <asm/percpu.h>

/* currently our limit due to fixed-size APID ID map */
#define APIC_MAX_PHYS_ID		254
#define APIC_INVALID_ID			255

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

#define APIC_EOI_ACK			0
#define APIC_ICR_VECTOR_MASK		0x000000ff
#define APIC_ICR_DLVR_MASK		0x00000700
#define  APIC_ICR_DLVR_FIXED		0x00000000
#define  APIC_ICR_DLVR_LOWPRI		0x00000100
#define  APIC_ICR_DLVR_SMI		0x00000200
#define  APIC_ICR_DLVR_NMI		0x00000400
#define  APIC_ICR_DLVR_INIT		0x00000500
#define  APIC_ICR_DLVR_SIPI		0x00000600
#define APIC_ICR_DEST_PHYSICAL		0x00000000
#define APIC_ICR_DEST_LOGICAL		0x00000800
#define APIC_ICR_DS_PENDING		0x00001000
#define APIC_ICR_LV_DEASSERT		0x00000000
#define APIC_ICR_LV_ASSERT		0x00004000
#define APIC_ICR_TM_EDGE		0x00000000
#define APIC_ICR_TM_LEVEL		0x00008000
#define APIC_ICR_SH_MASK		0x000c0000
#define  APIC_ICR_SH_NONE		0x00000000
#define  APIC_ICR_SH_SELF		0x00040000
#define  APIC_ICR_SH_ALL		0x00080000
#define  APIC_ICR_SH_ALLOTHER		0x000c0000

#define APIC_LVT_MASKED			0x00010000

#define X2APIC_DEST_LOGICAL_ID_MASK	0x0000ffff
#define X2APIC_DEST_CLUSTER_ID_MASK	0xffff0000
#define X2APIC_DEST_CLUSTER_ID_SHIFT	16

#define X2APIC_CLUSTER_ID_SHIFT		4

#define APIC_BSP_PSEUDO_SIPI		0x100

extern bool using_x2apic;

int apic_init(void);
int apic_cpu_init(struct per_cpu *cpu_data);

void apic_clear(void);

void apic_send_nmi_ipi(struct per_cpu *target_data);

void apic_nmi_handler(struct per_cpu *cpu_data);
void apic_irq_handler(struct per_cpu *cpu_data);

bool apic_handle_icr_write(struct per_cpu *cpu_data, u32 lo_val, u32 hi_val);

unsigned int apic_mmio_access(struct registers *guest_regs,
			      struct per_cpu *cpu_data, unsigned long rip,
			      unsigned long page_table_addr, unsigned int reg,
			      bool is_write);

void x2apic_handle_write(struct registers *guest_regs);
void x2apic_handle_read(struct registers *guest_regs);
