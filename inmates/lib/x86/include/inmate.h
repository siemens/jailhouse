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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _JAILHOUSE_INMATE_H
#define _JAILHOUSE_INMATE_H

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

static inline void enable_irqs(void)
{
	asm volatile("sti");
}

static inline void disable_irqs(void)
{
	asm volatile("cli");
}

static inline void cpu_relax(void)
{
	asm volatile("rep; nop" : : : "memory");
}

static inline void __attribute__((noreturn)) halt(void)
{
	while (1)
		asm volatile ("hlt" : : : "memory");
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

#define MAX_INTERRUPT_VECTORS	32

extern unsigned long idt[];
extern void *stack;

void excp_reporting_init(void);

void irq_send_ipi(unsigned int cpu_id, unsigned int vector);

enum ioapic_trigger_mode {
	TRIGGER_EDGE = 0,
	TRIGGER_LEVEL_ACTIVE_HIGH = 1 << 15,
	TRIGGER_LEVEL_ACTIVE_LOW = (1 << 15) | (1 << 13),
};

void ioapic_init(void);
void ioapic_pin_set_vector(unsigned int pin,
			   enum ioapic_trigger_mode trigger_mode,
			   unsigned int vector);

unsigned long long pm_timer_read(void);

unsigned long tsc_read_ns(void);
unsigned long tsc_init(void);

unsigned long apic_timer_init(unsigned int vector);
void apic_timer_set(unsigned long long timeout_ns);

extern volatile u32 smp_num_cpus;
extern u8 smp_cpu_ids[SMP_MAX_CPUS];
void smp_wait_for_all_cpus(void);
void smp_start_cpu(unsigned int cpu_id, void (*entry)(void));
#endif

#include <inmate_common.h>

#endif /* !_JAILHOUSE_INMATE_H */
