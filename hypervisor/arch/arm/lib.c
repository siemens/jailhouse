/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>

unsigned long long __aeabi_llsl(unsigned long long val, unsigned int shift);
unsigned long long __aeabi_llsr(unsigned long long val, unsigned int shift);

unsigned long long __aeabi_llsl(unsigned long long val, unsigned int shift)
{
	u32 lo = (u32)val << shift;
	u32 hi = ((u32)(val >> 32) << shift) | ((u32)val >> (32 - shift));

	return ((unsigned long long)hi << 32) | lo;
}

unsigned long long __aeabi_llsr(unsigned long long val, unsigned int shift)
{
	u32 lo = ((u32)val >> shift) | ((u32)(val >> 32) << (32 - shift));
	u32 hi = (u32)val >> shift;

	return ((unsigned long long)hi << 32) | lo;
}
