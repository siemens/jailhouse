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

#include <jailhouse/types.h>

/**
 * @ingroup IO
 * @defgroup IO-X86 x86
 * @{
 */

/**
 * Read 8 (b), 16(w) or 32-bit (l) value from a port.
 * @param port	Port number.
 *
 * @return Read value.
 * @{
 */
static inline u8 inb(u16 port)
{
	u8 v;

	asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

static inline u16 inw(u16 port)
{
	u16 v;

	asm volatile("inw %w1,%0" : "=a" (v) : "Nd" (port));
	return v;
}

static inline u32 inl(u16 port)
{
	u32 v;

	asm volatile("inl %1,%0" : "=a" (v) : "dN" (port));
	return v;
}
/** @} */

/**
 * Write 8 (b), 16(w) or 32-bit (l) value to a port.
 * @param value	Value to write
 * @param port	Port number.
 * @{
 */
static inline void outb(u8 value, u16 port)
{
	asm volatile("outb %0,%1" : : "a" (value), "dN" (port));
}

static inline void outw(u16 value, u16 port)
{
	asm volatile("outw %w0,%w1" : : "a" (value), "Nd" (port));
}

static inline void outl(u32 value, u16 port)
{
	asm volatile("outl %0,%1" : : "a" (value), "Nd" (port));
}
/** @} */

/** @} */
