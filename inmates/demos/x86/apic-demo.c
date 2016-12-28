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
 */

#include <inmate.h>

#define POLLUTE_CACHE_SIZE	(512 * 1024)

#define APIC_TIMER_VECTOR	32

static unsigned long expected_time;
static unsigned long min = -1, max;

static void irq_handler(void)
{
	unsigned long delta;

	delta = tsc_read() - expected_time;
	if (delta < min)
		min = delta;
	if (delta > max)
		max = delta;
	printk("Timer fired, jitter: %6ld ns, min: %6ld ns, max: %6ld ns\n",
	       delta, min, max);

	expected_time += 100 * NS_PER_MSEC;
	apic_timer_set(expected_time - tsc_read());
}

static void init_apic(void)
{
	unsigned long apic_freq_khz;

	int_init();
	int_set_handler(APIC_TIMER_VECTOR, irq_handler);

	apic_freq_khz = apic_timer_init(APIC_TIMER_VECTOR);
	printk("Calibrated APIC frequency: %lu kHz\n", apic_freq_khz);

	expected_time = tsc_read() + NS_PER_MSEC;
	apic_timer_set(NS_PER_MSEC);

	asm volatile("sti");
}

static void pollute_cache(void)
{
	char *mem = (char *)HEAP_BASE;
	unsigned long cpu_cache_line_size, ebx;
	unsigned long n;

	asm volatile("cpuid" : "=b" (ebx) : "a" (1)
		: "rcx", "rdx", "memory");
	cpu_cache_line_size = (ebx & 0xff00) >> 5;

	for (n = 0; n < POLLUTE_CACHE_SIZE; n += cpu_cache_line_size)
		mem[n] ^= 0xAA;
}

void inmate_main(void)
{
	bool allow_terminate = false;
	bool terminate = false;
	unsigned long tsc_freq;
	bool cache_pollution;

	comm_region->cell_state = JAILHOUSE_CELL_RUNNING_LOCKED;

	cache_pollution = cmdline_parse_bool("pollute-cache");
	if (cache_pollution)
		printk("Cache pollution enabled\n");

	tsc_freq = tsc_init();
	printk("Calibrated TSC frequency: %lu.%03u kHz\n", tsc_freq / 1000,
	       tsc_freq % 1000);

	init_apic();

	while (!terminate) {
		asm volatile("hlt");

		if (cache_pollution)
			pollute_cache();

		switch (comm_region->msg_to_cell) {
		case JAILHOUSE_MSG_SHUTDOWN_REQUEST:
			if (!allow_terminate) {
				printk("Rejecting first shutdown request - "
				       "try again!\n");
				jailhouse_send_reply_from_cell(comm_region,
						JAILHOUSE_MSG_REQUEST_DENIED);
				allow_terminate = true;
			} else
				terminate = true;
			break;
		default:
			jailhouse_send_reply_from_cell(comm_region,
					JAILHOUSE_MSG_UNKNOWN);
			break;
		}
	}

	printk("Stopped APIC demo\n");
	comm_region->cell_state = JAILHOUSE_CELL_SHUT_DOWN;
}
