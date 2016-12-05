/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_INMATE_H
#define _JAILHOUSE_INMATE_H

#define HEAP_BASE		0x000000
#define FSEGMENT_BASE		0x0f0000
#define COMM_REGION_BASE	0x100000

#define INMATE_CS32		0x8
#define INMATE_CS64		0x10
#define INMATE_DS32		0x18

#define PAGE_SIZE		(4 * 1024ULL)
#ifdef __x86_64__
#define BITS_PER_LONG		64
#define HUGE_PAGE_SIZE		(2 * 1024 * 1024ULL)
#else
#define BITS_PER_LONG		32
#define HUGE_PAGE_SIZE		(4 * 1024 * 1024ULL)
#endif
#define PAGE_MASK		(~(PAGE_SIZE - 1))
#define HUGE_PAGE_MASK		(~(HUGE_PAGE_SIZE - 1))

#define X2APIC_ID		0x802
#define X2APIC_ICR		0x830

#define APIC_LVL_ASSERT		(1 << 14)

#define PCI_CFG_VENDOR_ID	0x000
#define PCI_CFG_DEVICE_ID	0x002
#define PCI_CFG_COMMAND		0x004
# define PCI_CMD_IO		(1 << 0)
# define PCI_CMD_MEM		(1 << 1)
# define PCI_CMD_MASTER		(1 << 2)
# define PCI_CMD_INTX_OFF	(1 << 10)
#define PCI_CFG_STATUS		0x006
# define PCI_STS_INT		(1 << 3)
# define PCI_STS_CAPS		(1 << 4)
#define PCI_CFG_BAR		0x010
# define PCI_BAR_64BIT		0x4
#define PCI_CFG_CAP_PTR		0x034

#define PCI_ID_ANY		0xffff

#define PCI_DEV_CLASS_OTHER	0xff

#define PCI_CAP_MSI		0x05
#define PCI_CAP_MSIX		0x11

#define MSIX_CTRL_ENABLE	0x8000
#define MSIX_CTRL_FMASK		0x4000

#define SMP_MAX_CPUS		255

#ifndef __ASSEMBLY__
typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

static inline void cpu_relax(void)
{
	asm volatile("rep; nop" : : : "memory");
}

static inline void outb(u8 v, u16 port)
{
	asm volatile("outb %0,%1" : : "a" (v), "dN" (port));
}

static inline void outw(u16 v, u16 port)
{
	asm volatile("outw %0,%1" : : "a" (v), "dN" (port));
}

static inline void outl(u32 v, u16 port)
{
	asm volatile("outl %0,%1" : : "a" (v), "dN" (port));
}

static inline u8 inb(u16 port)
{
	u8 v;
	asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

static inline u16 inw(u16 port)
{
	u16 v;
	asm volatile("inw %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

static inline u32 inl(u16 port)
{
	u32 v;
	asm volatile("inl %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

static inline u8 mmio_read8(void *address)
{
	return *(volatile u8 *)address;
}

static inline u16 mmio_read16(void *address)
{
	return *(volatile u16 *)address;
}

static inline u32 mmio_read32(void *address)
{
	u32 value;

	/* assembly-encoded to match the hypervisor MMIO parser support */
	asm volatile("movl (%1),%0" : "=r" (value) : "r" (address));
	return value;
}

static inline u64 mmio_read64(void *address)
{
	return *(volatile u64 *)address;
}

static inline void mmio_write8(void *address, u8 value)
{
	*(volatile u8 *)address = value;
}

static inline void mmio_write16(void *address, u16 value)
{
	*(volatile u16 *)address = value;
}

static inline void mmio_write32(void *address, u32 value)
{
	/* assembly-encoded to match the hypervisor MMIO parser support */
	asm volatile("movl %0,(%1)" : : "r" (value), "r" (address));
}

static inline void mmio_write64(void *address, u64 value)
{
	*(volatile u64 *)address = value;
}

static inline u64 read_msr(unsigned int msr)
{
	u32 low, high;

	asm volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
	return low | ((u64)high << 32);
}

static inline void write_msr(unsigned int msr, u64 val)
{
	asm volatile("wrmsr"
		: /* no output */
		: "c" (msr), "a" ((u32)val), "d" ((u32)(val >> 32))
		: "memory");
}

static inline unsigned int cpu_id(void)
{
	return read_msr(X2APIC_ID);
}

typedef void(*int_handler_t)(void);

void int_init(void);
void int_set_handler(unsigned int vector, int_handler_t handler);
void int_send_ipi(unsigned int cpu_id, unsigned int vector);

enum ioapic_trigger_mode {
	TRIGGER_EDGE = 0,
	TRIGGER_LEVEL_ACTIVE_HIGH = 1 << 15,
	TRIGGER_LEVEL_ACTIVE_LOW = (1 << 15) | (1 << 13),
};

void ioapic_init(void);
void ioapic_pin_set_vector(unsigned int pin,
			   enum ioapic_trigger_mode trigger_mode,
			   unsigned int vector);

void hypercall_init(void);

unsigned long pm_timer_read(void);

unsigned long tsc_read(void);
unsigned long tsc_init(void);

void delay_us(unsigned long microsecs);

unsigned long apic_timer_init(unsigned int vector);
void apic_timer_set(unsigned long timeout_ns);

enum map_type { MAP_CACHED, MAP_UNCACHED };

void *alloc(unsigned long size, unsigned long align);
void map_range(void *start, unsigned long size, enum map_type map_type);

u32 pci_read_config(u16 bdf, unsigned int addr, unsigned int size);
void pci_write_config(u16 bdf, unsigned int addr, u32 value,
		      unsigned int size);
int pci_find_device(u16 vendor, u16 device, u16 start_bdf);
int pci_find_cap(u16 bdf, u16 cap);
void pci_msi_set_vector(u16 bdf, unsigned int vector);
void pci_msix_set_vector(u16 bdf, unsigned int vector, u32 index);

extern volatile u32 smp_num_cpus;
extern u8 smp_cpu_ids[SMP_MAX_CPUS];
void smp_wait_for_all_cpus(void);
void smp_start_cpu(unsigned int cpu_id, void (*entry)(void));
#endif

#include "../inmate_common.h"

#endif /* !_JAILHOUSE_INMATE_H */
