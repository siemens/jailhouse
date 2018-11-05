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

struct trap_context {
	unsigned long *regs;
	u32 hsr;
};

void access_cell_reg(struct trap_context *ctx, u8 reg, unsigned long *val,
		     bool is_read);
