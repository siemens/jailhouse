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

#define NS_PER_MSEC		1000000UL
#define NS_PER_SEC		1000000000UL

#define PM_TIMER_HZ		3579545
#define PM_TIMER_OVERFLOW	((0x1000000 * 1000000000ULL) / PM_TIMER_HZ)

static const unsigned int pm_timer_list[] = { 0x408, 0x1808, 0xb008, 0 };
static unsigned int pm_timer;

unsigned long read_pm_timer(void)
{
	static unsigned long last, overflows;
	unsigned long tmr;

	tmr = (inl(pm_timer) * NS_PER_SEC) / PM_TIMER_HZ;
	if (tmr < last)
		overflows += PM_TIMER_OVERFLOW;
	last = tmr;
	return tmr + overflows;
}

bool init_pm_timer(void)
{
	unsigned long val, loop;
	unsigned int n = 0;

	while (pm_timer_list[n]) {
		pm_timer = pm_timer_list[n++];
		val = read_pm_timer();
		for (loop = 0; loop < 10; loop++)
			cpu_relax();
		if (read_pm_timer() != val) {
			printk("Found PM Timer at %x\n", pm_timer);
			return true;
		}
	}
	printk("Could not find PM Timer\n");
	return false;
}
