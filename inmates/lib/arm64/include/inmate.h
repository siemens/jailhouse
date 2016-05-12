/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_INMATE_H
#define _JAILHOUSE_INMATE_H

#ifndef __ASSEMBLY__
typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

static inline u32 mmio_read32(void *address)
{
	return *(volatile u32 *)address;
}

static inline void mmio_write32(void *address, u32 value)
{
	*(volatile u32 *)address = value;
}

/*
 * To ease the debugging, we can send a spurious hypercall, which should return
 * -ENOSYS, but appear in the hypervisor stats for this cell.
 */
static inline void heartbeat(void)
{
	asm volatile (
	"mov	x0, %0\n"
	"hvc	#0\n"
	: : "r" (0xbea7) : "x0");
}

void __attribute__((used)) vector_irq(void);

typedef void (*irq_handler_t)(unsigned int);
void gic_setup(irq_handler_t handler);
void gic_enable_irq(unsigned int irq);

unsigned long timer_get_frequency(void);
u64 timer_get_ticks(void);
u64 timer_ticks_to_ns(u64 ticks);
void timer_start(u64 timeout);

#endif /* !__ASSEMBLY__ */

#include "../inmate_common.h"

#endif /* !_JAILHOUSE_INMATE_H */
