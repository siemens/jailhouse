/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define ARCH_SECTIONS							\
	. = ALIGN(PAGE_SIZE);						\
	.trampoline : {							\
		trampoline_start = .;					\
		*(.trampoline)						\
		trampoline_end = .;					\
	}
