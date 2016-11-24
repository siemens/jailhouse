/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2014-2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <mach/timer.h>
#include <inmate.h>

#define BEATS_PER_SEC		10

static u64 ticks_per_beat;
static volatile u64 expected_ticks;
static bool blinking_led;

static void handle_IRQ(unsigned int irqn)
{
	static u64 min_delta = ~0ULL, max_delta = 0;
	u64 delta;

	if (irqn != TIMER_IRQ)
		return;

	delta = timer_get_ticks() - expected_ticks;
	if (delta < min_delta)
		min_delta = delta;
	if (delta > max_delta)
		max_delta = delta;

	printk("Timer fired, jitter: %6ld ns, min: %6ld ns, max: %6ld ns\n",
	       (long)timer_ticks_to_ns(delta),
	       (long)timer_ticks_to_ns(min_delta),
	       (long)timer_ticks_to_ns(max_delta));

	if (blinking_led) {
#ifdef CONFIG_MACH_SUN7I
		/* let green LED on Banana Pi blink */
		#define LED_REG (void *)(0x1c20800 + 7*0x24 + 0x10)
		mmio_write32(LED_REG, mmio_read32(LED_REG) ^ (1<<24));
#endif
	}

	expected_ticks = timer_get_ticks() + ticks_per_beat;
	timer_start(ticks_per_beat);
}

void inmate_main(void)
{
	printk("Initializing the GIC...\n");
	gic_setup(handle_IRQ);
	gic_enable_irq(TIMER_IRQ);

	printk("Initializing the timer...\n");
	ticks_per_beat = timer_get_frequency() / BEATS_PER_SEC;
	expected_ticks = timer_get_ticks() + ticks_per_beat;
	timer_start(ticks_per_beat);

	blinking_led = cmdline_parse_bool("blinking_led");

	while (1)
		asm volatile("wfi" : : : "memory");
}
