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

#include <inmate.h>

#define X86_FEATURE_VMX	(1 << 5)

bool jailhouse_use_vmcall;

void hypercall_init(void)
{
	u32 eax = 1, ecx = 0;

	asm volatile(
		"cpuid"
		: "=c" (ecx)
		: "a" (eax), "c" (ecx)
		: "rbx", "rdx", "memory"
	);

	if (ecx & X86_FEATURE_VMX)
		jailhouse_use_vmcall = true;
}
