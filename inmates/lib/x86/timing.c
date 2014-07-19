/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

#define PM_TIMER_HZ		3579545
#define PM_TIMER_OVERFLOW	((0x1000000 * 1000000000ULL) / PM_TIMER_HZ)

#define X2APIC_LVTT		0x832
#define X2APIC_TMICT		0x838
#define X2APIC_TMCCT		0x839
#define X2APIC_TDCR		0x83e

static unsigned long divided_apic_freq;

unsigned long pm_timer_read(void)
{
	static unsigned long last, overflows;
	unsigned long tmr;

	tmr = (inl(comm_region->pm_timer_address) * NS_PER_SEC) / PM_TIMER_HZ;
	if (tmr < last)
		overflows += PM_TIMER_OVERFLOW;
	last = tmr;
	return tmr + overflows;
}

void delay_us(unsigned long microsecs)
{
	unsigned long timeout = pm_timer_read() + microsecs * NS_PER_USEC;

	while ((long)(timeout - pm_timer_read()) > 0)
		cpu_relax();
}

unsigned long apic_timer_init(unsigned int vector)
{
	unsigned long start, end;
	unsigned long tmr;

	write_msr(X2APIC_TDCR, 3);

	start = pm_timer_read();
	write_msr(X2APIC_TMICT, 0xffffffff);

	while (pm_timer_read() - start < 100 * NS_PER_MSEC)
		cpu_relax();

	end = pm_timer_read();
	tmr = read_msr(X2APIC_TMCCT);

	divided_apic_freq = (0xffffffffULL - tmr) * NS_PER_SEC / (end - start);

	write_msr(X2APIC_TMICT, 0);
	write_msr(X2APIC_LVTT, vector);

	return (divided_apic_freq * 16 + 500) / 1000;
}

void apic_timer_set(unsigned long timeout_ns)
{
	unsigned long long ticks =
		(unsigned long long)timeout_ns * divided_apic_freq;
	write_msr(X2APIC_TMICT, ticks / NS_PER_SEC);
}
