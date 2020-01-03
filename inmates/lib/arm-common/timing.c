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

void delay_us(unsigned long microsecs)
{
	unsigned long long timeout = timer_get_ticks() +
		microsecs * (timer_get_frequency() / 1000 / 1000);

	while ((long long)(timeout - timer_get_ticks()) > 0)
		cpu_relax();
}
