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

#define FSEGMENT_BASE	0xf0000

#define INMATE_CS32	0x8
#define INMATE_CS64	0x10
#define INMATE_DS32	0x18

#ifndef __ASSEMBLY__
typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long s64;
typedef unsigned long u64;

typedef s8 __s8;
typedef u8 __u8;

typedef s16 __s16;
typedef u16 __u16;

typedef s32 __s32;
typedef u32 __u32;

typedef s64 __s64;
typedef u64 __u64;

typedef enum { true=1, false=0 } bool;

#include <jailhouse/hypercall.h>

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

extern unsigned int printk_uart_base;
void printk(const char *fmt, ...);

void *memset(void *s, int c, unsigned long n);

extern u8 irq_entry[];
void irq_handler(void);

void inmate_main(void);

unsigned long read_pm_timer(struct jailhouse_comm_region *comm_region);
#endif
