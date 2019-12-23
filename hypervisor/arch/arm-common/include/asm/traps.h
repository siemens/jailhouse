/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2018
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

enum trap_return {
	TRAP_HANDLED		= 1,
	TRAP_UNHANDLED		= 0,
	TRAP_FORBIDDEN		= -1,
};

typedef enum trap_return (*trap_handler)(struct trap_context *ctx);

void arch_skip_instruction(struct trap_context *ctx);

enum trap_return arch_handle_dabt(struct trap_context *ctx);
