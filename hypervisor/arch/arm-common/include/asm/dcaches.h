/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef __ASSEMBLY__

struct cell;

enum dcache_flush {
	DCACHE_CLEAN,
	DCACHE_INVALIDATE,
	DCACHE_CLEAN_AND_INVALIDATE,
};

void arm_dcaches_flush(void *addr, long size, enum dcache_flush flush);
void arm_cell_dcaches_flush(struct cell *cell, enum dcache_flush flush);

#endif /* !__ASSEMBLY__ */
