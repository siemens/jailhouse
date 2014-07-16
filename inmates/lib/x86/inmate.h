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

#define FSEGMENT_BASE		0x0f0000
#define COMM_REGION_BASE	0x100000

#define INMATE_CS32		0x8
#define INMATE_CS64		0x10
#define INMATE_DS32		0x18

#define NS_PER_MSEC		1000000UL
#define NS_PER_SEC		1000000000UL

#define X2APIC_ID		0x802

#ifndef __ASSEMBLY__
typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

typedef s8 __s8;
typedef u8 __u8;

typedef s16 __s16;
typedef u16 __u16;

typedef s32 __s32;
typedef u32 __u32;

typedef s64 __s64;
typedef u64 __u64;

typedef enum { true=1, false=0 } bool;

static inline void cpu_relax(void)
{
	asm volatile("rep; nop" : : : "memory");
}

static inline void outb(u8 v, u16 port)
{
	asm volatile("outb %0,%1" : : "a" (v), "dN" (port));
}

static inline u8 inb(u16 port)
{
	u8 v;
	asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

static inline u32 inl(u16 port)
{
	u32 v;
	asm volatile("inl %1,%0" : "=a" (v) : "dN" (port));
	return v;
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
		: "c" (msr), "a" (val), "d" (val >> 32)
		: "memory");
}

static inline unsigned int cpu_id(void)
{
	return read_msr(X2APIC_ID);
}

#include <jailhouse/hypercall.h>

#define comm_region	((struct jailhouse_comm_region *)COMM_REGION_BASE)

extern unsigned int printk_uart_base;
void printk(const char *fmt, ...);

void *memset(void *s, int c, unsigned long n);

typedef void(*int_handler_t)(void);

void int_init(void);
void int_set_handler(unsigned int vector, int_handler_t handler);

void inmate_main(void);

unsigned long pm_timer_read(void);
unsigned long apic_timer_init(unsigned int vector);
void apic_timer_set(unsigned long timeout_ns);
#endif
