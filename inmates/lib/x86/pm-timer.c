/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

#define NS_PER_SEC		1000000000UL

#define PM_TIMER_HZ		3579545
#define PM_TIMER_OVERFLOW	((0x1000000 * 1000000000ULL) / PM_TIMER_HZ)

unsigned long read_pm_timer(void)
{
	static unsigned long last, overflows;
	unsigned long tmr;

	tmr = (inl(comm_region->pm_timer_address) * NS_PER_SEC) / PM_TIMER_HZ;
	if (tmr < last)
		overflows += PM_TIMER_OVERFLOW;
	last = tmr;
	return tmr + overflows;
}
