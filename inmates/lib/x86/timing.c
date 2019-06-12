/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inmate.h>

#define PM_TIMER_HZ		3579545
#define PM_TIMER_OVERFLOW      ((0x1000000 * NS_PER_SEC) / PM_TIMER_HZ)

#define IA32_TSC_DEADLINE	0x6e0

#define X2APIC_LVTT		0x832
# define LVTT_TSC_DEADLINE	(1 << 18)
#define X2APIC_TMICT		0x838
#define X2APIC_TMCCT		0x839
#define X2APIC_TDCR		0x83e

static unsigned long apic_tick_freq;
static unsigned long long pm_timer_last[SMP_MAX_CPUS];
static unsigned long pm_timer_overflows[SMP_MAX_CPUS];
static unsigned long tsc_freq, tsc_overflow;
static unsigned long tsc_last[SMP_MAX_CPUS];
static unsigned long tsc_overflows[SMP_MAX_CPUS];
static bool tsc_deadline;

static u64 rdtsc(void)
{
#ifdef __x86_64__
	u32 lo, hi;

	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return (u64)lo | (((u64)hi) << 32);
#else
	u64 v;

	asm volatile("rdtsc" : "=A" (v));
	return v;
#endif
}

unsigned long tsc_read_ns(void)
{
	unsigned int cpu = cpu_id();
	unsigned long tmr;

	tmr = ((rdtsc() & 0xffffffffLL) * NS_PER_SEC) / tsc_freq;
	if (tmr < tsc_last[cpu])
		tsc_overflows[cpu] += tsc_overflow;
	tsc_last[cpu] = tmr;
	return tmr + tsc_overflows[cpu];
}

unsigned long tsc_init(void)
{
	tsc_freq = comm_region->tsc_khz * 1000L;
	tsc_overflow = (0x100000000L * NS_PER_SEC) / tsc_freq;

	return tsc_freq;
}

unsigned long long pm_timer_read(void)
{
	unsigned int cpu = cpu_id();
	unsigned long long tmr;

	tmr = ((unsigned long long)(inl(comm_region->pm_timer_address)
		& 0x00ffffff) * NS_PER_SEC) / PM_TIMER_HZ;
	if (tmr < pm_timer_last[cpu])
		pm_timer_overflows[cpu] += PM_TIMER_OVERFLOW;
	pm_timer_last[cpu] = tmr;
	return tmr + pm_timer_overflows[cpu];
}

void delay_us(unsigned long microsecs)
{
	unsigned long long timeout = pm_timer_read() + microsecs * NS_PER_USEC;

	while ((long long)(timeout - pm_timer_read()) > 0)
		cpu_relax();
}

unsigned long apic_timer_init(unsigned int vector)
{
	unsigned long apic_freq;
	unsigned long ecx;

	asm volatile("cpuid" : "=c" (ecx) : "a" (1)
		: "rbx", "rdx", "memory");
	tsc_deadline = !!(ecx & (1 << 24));

	if (tsc_deadline) {
		vector |= LVTT_TSC_DEADLINE;
		apic_tick_freq = tsc_init();
		apic_freq = apic_tick_freq / 1000;
	} else {
		apic_tick_freq = comm_region->apic_khz * 1000 / 16;
		apic_freq = comm_region->apic_khz;
	}

	write_msr(X2APIC_LVTT, vector);

	/* Required when using TSC deadline mode. */
	asm volatile("mfence" : : : "memory");

	return apic_freq;
}

void apic_timer_set(unsigned long long timeout_ns)
{
	unsigned long long ticks = timeout_ns * apic_tick_freq / NS_PER_SEC;

	if (tsc_deadline)
		write_msr(IA32_TSC_DEADLINE, rdtsc() + ticks);
	else
		write_msr(X2APIC_TMICT, ticks);
}
