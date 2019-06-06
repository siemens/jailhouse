/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/paging.h>

/**
 * @ingroup IO
 * @addtogroup IO-X86 x86
 * @{
 */

/** Information about MMIO instruction performing an access. */
struct mmio_instruction {
	/** Length of the MMIO access instruction, 0 for invalid or unsupported
	 * access. */
	unsigned int inst_len;
	/** Size of the access. */
	unsigned int access_size;
	/** Number of the register that should receive the input. */
	unsigned int in_reg_num;
	/** Output value, already copied either from a register or
         * from an immediate value */
	unsigned long out_val;
	/** A read must not clear the upper bits of registers, if the access
	 * width is smaller than 32 bit. This mask describes the bits that have
	 * to be preserved.
	 */
	unsigned long reg_preserve_mask;
};

/**
 * Parse instruction causing an intercepted MMIO access on a cell CPU.
 * @param pg_structs	Currently active guest (cell) paging structures.
 * @param is_write	True if write access, false for read.
 *
 * @return MMIO instruction information. mmio_instruction::inst_len is 0 on
 * 	   invalid or unsupported access.
 */
struct mmio_instruction
x86_mmio_parse(const struct guest_paging_structures *pg_structs, bool is_write);

/** @} */
