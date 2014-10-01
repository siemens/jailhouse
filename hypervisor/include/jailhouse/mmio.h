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

/**
 * @defgroup IO I/O Access Subsystem
 *
 * This subsystem provides accessors to I/O and supports the interpretation of
 * intercepted I/O accesses of cells.
 *
 * @{
 */

/** Information about MMIO instruction performing an access. */
struct mmio_instruction {
	/** Length of the MMIO access instruction, 0 for invalid or unsupported
	 * access. */
	unsigned int inst_len;
	/** Size of the access. */
	unsigned int access_size;
	/** Architecture-specific number of the register that holds the output
	 * value or should receive the input. */
	unsigned int reg_num;
};

/**
 * Define MMIO read accessor.
 * @param size		Access size.
 */
#define DEFINE_MMIO_READ(size)						\
static inline u##size mmio_read##size(void *address)			\
{									\
	return *(volatile u##size *)address;				\
}

/**
 * Read 8, 16, 32 or 64-bit value from a memory-mapped I/O register.
 * @param address	Virtual address of the register.
 *
 * @return Read value.
 * @{
 */
DEFINE_MMIO_READ(8)
DEFINE_MMIO_READ(16)
DEFINE_MMIO_READ(32)
DEFINE_MMIO_READ(64)
/** @} */

/**
 * Define MMIO write accessor.
 * @param size		Access size.
 */
#define DEFINE_MMIO_WRITE(size)						\
static inline void mmio_write##size(void *address, u##size value)	\
{									\
	*(volatile u##size *)address = value;				\
}

/**
 * Write 8, 16, 32 or 64-bit value to a memory-mapped I/O register.
 * @param address	Virtual address of the register.
 * @param value		Value to write.
 * @{
 */
DEFINE_MMIO_WRITE(8)
DEFINE_MMIO_WRITE(16)
DEFINE_MMIO_WRITE(32)
DEFINE_MMIO_WRITE(64)
/** @} */

/**
 * Read value from 32 or 64-bit MMIO register field.
 * @param address	Virtual address of the register.
 * @param mask		Bitmask to defining the field. Only successive bits
 * 			must be set.
 *
 * @return Field value of register, shifted so that the first non-zero bit in
 * 	   @c mask becomes bit 0.
 * @{
 */
static inline u32 mmio_read32_field(void *address, u32 mask)
{
	return (mmio_read32(address) & mask) >> (__builtin_ffs(mask) - 1);
}

static inline u64 mmio_read64_field(void *address, u64 mask)
{
	return (mmio_read64(address) & mask) >> (__builtin_ffsl(mask) - 1);
}
/** @} */

/**
 * Write value to 32 or 64-bit MMIO register field.
 * @param address	Virtual address of the register.
 * @param mask		Bitmask to defining the field. Only successive bits
 * 			must be set.
 * @param value		Register field value.
 *
 * This updates only the field value of the register, leaving all other
 * register bits unmodified. Thus, it performs a read-modify-write cycle.
 * @{
 */
static inline void mmio_write32_field(void *address, u32 mask, u32 value)
{
	mmio_write32(address, (mmio_read32(address) & ~mask) |
			((value << (__builtin_ffs(mask) - 1)) & mask));
}

static inline void mmio_write64_field(void *address, u64 mask, u64 value)
{
	mmio_write64(address, (mmio_read64(address) & ~mask) |
			((value << (__builtin_ffsl(mask) - 1)) & mask));
}
/** @} */

/**
 * Parse instruction causing an intercepted MMIO access on a cell CPU.
 * @param pc		Program counter of the access instruction.
 * @param pg_structs	Currently active guest (cell) paging structures.
 * @param is_write	True if write access, false for read.
 *
 * @return MMIO instruction information. mmio_instruction::inst_len is 0 on
 * 	   invalid or unsupported access.
 */
struct mmio_instruction
mmio_parse(unsigned long pc, const struct guest_paging_structures *pg_structs,
	   bool is_write);

/** @} */
#endif /* !_JAILHOUSE_MMIO_H */
