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
	OFFSET(HEADER_HYP_STUB_VERSION, jailhouse_header, arm_linux_hyp_abi);
	OFFSET(SYSCONFIG_DEBUG_CONSOLE_PHYS, jailhouse_system,
	       debug_console.address);
	OFFSET(SYSCONFIG_HYPERVISOR_PHYS, jailhouse_system,
	       hypervisor_memory.phys_start);
	OFFSET(PERCPU_ID_AA64MMFR0, per_cpu, id_aa64mmfr0);
	BLANK();

	DEFINE(PERCPU_STACK_END,
	       __builtin_offsetof(struct per_cpu, stack) + \
	       FIELD_SIZEOF(struct per_cpu, stack));
	DEFINE(PERCPU_SIZE_ASM, sizeof(struct per_cpu));
	BLANK();

	DEFINE(DCACHE_CLEAN_ASM, DCACHE_CLEAN);
	DEFINE(DCACHE_INVALIDATE_ASM, DCACHE_INVALIDATE);
	DEFINE(DCACHE_CLEAN_AND_INVALIDATE_ASM, DCACHE_CLEAN_AND_INVALIDATE);
}
