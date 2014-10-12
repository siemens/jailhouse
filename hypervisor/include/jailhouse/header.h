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

#define JAILHOUSE_SIGNATURE	"JAILHOUS"

/**
 * @ingroup Setup
 * @{
 */

/**
 * Hypervisor entry point.
 *
 * @see arch_entry
 */
typedef int (*jailhouse_entry)(unsigned int);

/** Hypervisor description. */
struct jailhouse_header {
	/** Signature "JAILHOUS".
	 * @note Filled at build time. */
	char signature[8];
	/** Size of hypervisor core.
	 * @note Filled at build time. */
	unsigned long core_size;
	/** Size of per-CPU data structure.
	 * @note Filled at build time. */
	unsigned long percpu_size;
	/** Entry point (arch_entry()).
	 * @note Filled at build time. */
	int (*entry)(unsigned int);

	/** Configured maximum logical CPU ID + 1.
	 * @note Filled by Linux loader driver before entry. */
	unsigned int max_cpus;
	/** Number of online CPUs that will call the entry function.
	 * @note Filled by Linux loader driver before entry. */
	unsigned int online_cpus;
	/** Virtual base address of debug UART (if used).
	 * @note Filled by Linux loader driver before entry. */
	void *debug_uart_base;
};
