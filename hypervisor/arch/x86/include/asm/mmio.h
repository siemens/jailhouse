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
	/** Number of the register that holds the output value or should
	 * receive the input. */
	unsigned int reg_num;
};

/**
 * Parse instruction causing an intercepted MMIO access on a cell CPU.
 * @param pc		Program counter of the access instruction.
 * @param pg_structs	Currently active guest (cell) paging structures.
 * @param is_write	True if write access, false for read.
 *
 * @return MMIO instruction information. mmio_instruction::inst_len is 0 on
 * 	   invalid or unsupported access.
 */
struct mmio_instruction x86_mmio_parse(unsigned long pc,
	const struct guest_paging_structures *pg_structs, bool is_write);

/** @} */
