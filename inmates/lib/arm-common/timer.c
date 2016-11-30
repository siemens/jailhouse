/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/sysregs.h>
#include <inmate.h>

unsigned long timer_get_frequency(void)
{
	unsigned long freq;

	arm_read_sysreg(CNTFRQ_EL0, freq);
	return freq;
}

u64 timer_get_ticks(void)
{
	u64 pct64;

	arm_read_sysreg(CNTPCT_EL0, pct64);
	return pct64;
}

static unsigned long emul_division(u64 val, u64 div)
{
	unsigned long cnt = 0;

	while (val > div) {
		val -= div;
		cnt++;
	}
	return cnt;
}

u64 timer_ticks_to_ns(u64 ticks)
{
	return emul_division(ticks * 1000,
			     timer_get_frequency() / 1000 / 1000);
}

void timer_start(u64 timeout)
{
	arm_write_sysreg(CNTV_TVAL_EL0, timeout);
	arm_write_sysreg(CNTV_CTL_EL0, 1);
}
