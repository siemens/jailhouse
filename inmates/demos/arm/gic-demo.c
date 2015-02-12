/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/gic_common.h>
#include <asm/sysregs.h>
#include <inmates/inmate.h>
#include <mach/timer.h>

#define BEATS_PER_SEC		10
#define TICKS_PER_BEAT		(TIMER_FREQ / BEATS_PER_SEC)

static volatile u64 expected_ticks;

static void timer_arm(u64 timeout)
{
	arm_write_sysreg(CNTV_TVAL_EL0, timeout);
	arm_write_sysreg(CNTV_CTL_EL0, 1);
}

static u64 get_actual_ticks(void)
{
	u64 pct64;

	arm_read_sysreg(CNTPCT, pct64);
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

static inline unsigned long ticks_to_ns(u64 ticks)
{
	return emul_division(ticks * 1000, TIMER_FREQ / 1000 / 1000);
}

static void handle_IRQ(unsigned int irqn)
{
	static u64 min_delta = ~0ULL, max_delta = 0;
	u64 delta;

	if (irqn != TIMER_IRQ)
		return;

	delta = get_actual_ticks() - expected_ticks;
	if (delta < min_delta)
		min_delta = delta;
	if (delta > max_delta)
		max_delta = delta;

	printk("Timer fired, jitter: %6ld ns, min: %6ld ns, max: %6ld ns\n",
	       ticks_to_ns(delta), ticks_to_ns(min_delta),
	       ticks_to_ns(max_delta));

#ifdef CONFIG_MACH_SUN7I
	/* let green LED on Banana Pi blink */
	#define LED_REG (void *)(0x1c20800 + 7*0x24 + 0x10)
	mmio_write32(LED_REG, mmio_read32(LED_REG) ^ (1<<24));
#endif

	expected_ticks = get_actual_ticks() + TICKS_PER_BEAT;
	timer_arm(TICKS_PER_BEAT);
}

void inmate_main(void)
{
	printk("Initializing the GIC...\n");
	gic_setup(handle_IRQ);
	gic_enable_irq(TIMER_IRQ);

	printk("Initializing the timer...\n");
	expected_ticks = get_actual_ticks() + TICKS_PER_BEAT;
	timer_arm(TICKS_PER_BEAT);

	while (1)
		asm volatile("wfi" : : : "memory");
}
