/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/paging.h>
#include <jailhouse/gen-defines.h>
#include <asm/percpu.h>

void common(void);

void common(void)
{
	OFFSET(HEADER_MAX_CPUS, jailhouse_header, max_cpus);
	OFFSET(HEADER_DEBUG_CONSOLE_VIRT, jailhouse_header, debug_console_base);
	OFFSET(SYSCONFIG_DEBUG_CONSOLE_PHYS, jailhouse_system,
	       debug_console.address);
	OFFSET(SYSCONFIG_HYPERVISOR_PHYS, jailhouse_system,
	       hypervisor_memory.phys_start);
	BLANK();

	DEFINE(PERCPU_STACK_END,
	       __builtin_offsetof(struct per_cpu, stack) + \
	       FIELD_SIZEOF(struct per_cpu, stack));
	DEFINE(PERCPU_SIZE_SHIFT_ASM, PERCPU_SIZE_SHIFT);
	OFFSET(PERCPU_SAVED_VECTORS, per_cpu, saved_vectors);
	BLANK();

	DEFINE(DCACHE_CLEAN_ASM, DCACHE_CLEAN);
	DEFINE(DCACHE_INVALIDATE_ASM, DCACHE_INVALIDATE);
	DEFINE(DCACHE_CLEAN_AND_INVALIDATE_ASM, DCACHE_CLEAN_AND_INVALIDATE);
}
