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

#ifndef _JAILHOUSE_MMIO_H
#define _JAILHOUSE_MMIO_H

#include <jailhouse/paging.h>

struct mmio_access {
	unsigned int inst_len;
	unsigned int size;
	unsigned int reg;
};

#define DEFINE_MMIO_READ(size)						\
static inline u##size mmio_read##size(void *address)			\
{									\
	return *(volatile u##size *)address;				\
}
DEFINE_MMIO_READ(8)
DEFINE_MMIO_READ(16)
DEFINE_MMIO_READ(32)
DEFINE_MMIO_READ(64)

#define DEFINE_MMIO_WRITE(size)						\
static inline void mmio_write##size(void *address, u##size value)	\
{									\
	*(volatile u##size *)address = value;				\
}
DEFINE_MMIO_WRITE(8)
DEFINE_MMIO_WRITE(16)
DEFINE_MMIO_WRITE(32)
DEFINE_MMIO_WRITE(64)

struct mmio_access mmio_parse(unsigned long pc,
			      const struct guest_paging_structures *pg_structs,
			      bool is_write);

/**
 * mmio_read32_field() - Read value of 32-bit register field
 * @addr:	Register address.
 * @mask:	Bit mask. Shifted value must be provided which describes both
 * 		starting bit position (1st non-zero bit) and length of the field.
 *
 * Return: Field value of register.
 */
static inline u32 mmio_read32_field(void *addr, u32 mask)
{
	return (mmio_read32(addr) & mask) >> (__builtin_ffs(mask) - 1);
}

/**
 * mmio_write32_field() - Write value of 32-bit register field
 * @addr:	Register address.
 * @mask:	Bit mask. See mmio_read32_field() for more details.
 * @value:	Register field value (must be the same length as mask).
 *
 * Update field value of 32-bit register, leaving all other fields unmodified.

 * Return: None.
 */
static inline void mmio_write32_field(void *addr, u32 mask, u32 value)
{
	mmio_write32(addr, (mmio_read32(addr) & ~mask) |
			((value << (__builtin_ffs(mask) - 1)) & mask));
}

/**
 * mmio_read64_field() - Read value of 64-bit register field.
 * See mmio_read32_field() for more details.
 */
static inline u64 mmio_read64_field(void *addr, u64 mask)
{
	return (mmio_read64(addr) & mask) >> (__builtin_ffsl(mask) - 1);
}

/**
 * mmio_write64_field() - Write value of 64-bit register field.
 * See mmio_write32_field() for more details.
 */
static inline void mmio_write64_field(void *addr, u64 mask, u64 value)
{
	mmio_write64(addr, (mmio_read64(addr) & ~mask) |
			((value << (__builtin_ffsl(mask) - 1)) & mask));
}

#endif /* !_JAILHOUSE_MMIO_H */
