/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define NUM_ENTRY_REGS			13

#define ARM_PERCPU_FIELDS						\
	/** Linux stack pointer, used for handover to the hypervisor. */ \
	unsigned long linux_sp;						\
	unsigned long linux_ret;					\
	unsigned long linux_flags;					\
	unsigned long linux_reg[NUM_ENTRY_REGS];			\
									\
	bool initialized;
