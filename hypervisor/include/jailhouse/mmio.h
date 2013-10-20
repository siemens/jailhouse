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

struct mmio_access {
	unsigned int inst_len;
	unsigned int size;
	unsigned int reg;
};

static inline u32 mmio_read32(void *address)
{
	return *(volatile u32 *)address;
}

static inline u64 mmio_read64(void *address)
{
	return *(volatile u64 *)address;
}

static inline void mmio_write32(void *address, u32 value)
{
	*(volatile u32 *)address = value;
}

static inline void mmio_write64(void *address, u64 value)
{
	*(volatile u64 *)address = value;
}

struct mmio_access mmio_parse(struct per_cpu *cpu_data, unsigned long pc,
			      unsigned long page_table_addr, bool is_write);
