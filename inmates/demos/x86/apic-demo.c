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
#include <jailhouse/hypercall.h>

#ifdef CONFIG_UART_OXPCIE952
#define UART_BASE		0xe010
#else
#define UART_BASE		0x3f8
#endif
#define UART_LSR		0x5
#define UART_LSR_THRE		0x20
#define UART_IDLE_LOOPS		100

#define APIC_TIMER_VECTOR	32

static unsigned long expected_time;
static unsigned long min = -1, max;

static void irq_handler(void)
{
	unsigned long delta;

	delta = pm_timer_read() - expected_time;
	if (delta < min)
		min = delta;
	if (delta > max)
		max = delta;
	printk("Timer fired, jitter: %6ld ns, min: %6ld ns, max: %6ld ns\n",
	       delta, min, max);

	expected_time += 100 * NS_PER_MSEC;
	apic_timer_set(expected_time - pm_timer_read());
}

static void init_apic(void)
{
	unsigned long apic_freq_khz;

	int_init();
	int_set_handler(APIC_TIMER_VECTOR, irq_handler);

	apic_freq_khz = apic_timer_init(APIC_TIMER_VECTOR);
	printk("Calibrated APIC frequency: %lu kHz\n", apic_freq_khz);

	expected_time = pm_timer_read() + NS_PER_MSEC;
	apic_timer_set(NS_PER_MSEC);

	asm volatile("sti");
}

void inmate_main(void)
{
	bool allow_terminate = false;
	bool terminate = false;
	unsigned int n;

	printk_uart_base = UART_BASE;
	do {
		for (n = 0; n < UART_IDLE_LOOPS; n++)
			if (!(inb(UART_BASE + UART_LSR) & UART_LSR_THRE))
				break;
	} while (n < UART_IDLE_LOOPS);

	comm_region->cell_state = JAILHOUSE_CELL_RUNNING_LOCKED;

	init_apic();

	while (!terminate) {
		asm volatile("hlt");

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
