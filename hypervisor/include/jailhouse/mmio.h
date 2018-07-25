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

#ifndef _JAILHOUSE_MMIO_H
#define _JAILHOUSE_MMIO_H

#include <jailhouse/types.h>
#include <asm/mmio.h>
#include <jailhouse/cell-config.h>

struct cell;

/**
 * @defgroup IO I/O Access Subsystem
 *
 * This subsystem provides accessors to I/O and supports the interpretation and
 * handling of intercepted I/O accesses performed by cells.
 *
 * @{
 */

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

/** MMIO access result. */
enum mmio_result {MMIO_ERROR = -1, MMIO_UNHANDLED, MMIO_HANDLED};

/** MMIO access description. */
struct mmio_access {
	/** Address to access, depending on the context, an absolute address or
	 * relative offset to region start. */
	unsigned long address;
	/** Size of the access. */
	unsigned int size;
	/** True if write access. */
	bool is_write;
	/** The value to be written or the read value to return. */
	unsigned long value;
};

/** MMIO handler.
 * @param arg		Opaque argument defined via mmio_region_register().
 * @param mmio		MMIO access description. @a mmio->address will be
 * 			provided as offset to the region start.
 *
 * @return MMIO_HANDLED on success, MMIO_ERROR otherwise.
 */
typedef enum mmio_result (*mmio_handler)(void *arg, struct mmio_access *mmio);

/** MMIO region coordinates. */
struct mmio_region_location {
	/** Start address of the region. */
	unsigned long start;
	/** Region size. */
	unsigned long size;
};

/** MMIO region access handler description. */
struct mmio_region_handler {
	/** Access handling function. */
	mmio_handler function;
	/** Argument to pass to the function. */
	void *arg;
};

int mmio_cell_init(struct cell *cell);

void mmio_region_register(struct cell *cell, unsigned long start,
			  unsigned long size, mmio_handler handler,
			  void *handler_arg);
void mmio_region_unregister(struct cell *cell, unsigned long start);

enum mmio_result mmio_handle_access(struct mmio_access *mmio);

void mmio_cell_exit(struct cell *cell);

void mmio_perform_access(void *base, struct mmio_access *mmio);

int mmio_subpage_register(struct cell *cell,
			  const struct jailhouse_memory *mem);
void mmio_subpage_unregister(struct cell *cell,
			     const struct jailhouse_memory *mem);

/** @} */
#endif /* !_JAILHOUSE_MMIO_H */
