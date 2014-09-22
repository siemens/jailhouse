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

typedef int (*jailhouse_entry)(unsigned int);

struct jailhouse_header {
	/* filled at build time */
	__u8	signature[8];
	__u64	core_size;
	__u64	percpu_size;

	/* jailhouse_entry */
	__u64	entry;

	/* filled by loader */
	__u32	max_cpus;
	__u32	online_cpus;
};
