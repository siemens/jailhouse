/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

struct mmio_access {
	unsigned long addr;
	bool is_write;
	unsigned int size;
	unsigned long val;
};

int arch_mmio_access(struct mmio_access *access);
