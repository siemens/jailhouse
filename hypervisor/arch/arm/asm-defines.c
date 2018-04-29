/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/gen-defines.h>
#include <jailhouse/utils.h>
#include <asm/percpu.h>

void common(void);

void common(void)
{
	OFFSET(PERCPU_LINUX_SP, per_cpu, linux_sp);
	BLANK();

	/* GCC evaluates constant expressions involving built-ins
	 * at compilation time, so this yields computed value.
	 */
	DEFINE(PERCPU_STACK_END,
	       __builtin_offsetof(struct per_cpu, stack) + \
	       FIELD_SIZEOF(struct per_cpu, stack));
	DEFINE(PERCPU_SIZE_ASM, sizeof(struct per_cpu));
}
