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

#define NS_PER_MSEC		1000000UL
#define NS_PER_SEC		1000000000UL

#define APIC_TIMER_VECTOR	32

#define X2APIC_LVTT		0x832
#define X2APIC_TMICT		0x838
#define X2APIC_TMCCT		0x839
#define X2APIC_TDCR		0x83e

static unsigned long apic_frequency;
static unsigned long expected_time;
static unsigned long min = -1, max;

static void irq_handler(void)
{
	unsigned long delta;

	delta = read_pm_timer() - expected_time;
	if (delta < min)
		min = delta;
	if (delta > max)
		max = delta;
	printk("Timer fired, jitter: %6ld ns, min: %6ld ns, max: %6ld ns\n",
	       delta, min, max);

	expected_time += 100 * NS_PER_MSEC;
	write_msr(X2APIC_TMICT, (expected_time - read_pm_timer()) *
				apic_frequency / NS_PER_SEC);
}

static void init_apic(void)
{
	unsigned long start, end;
	unsigned long tmr;

	int_init();
	int_set_handler(APIC_TIMER_VECTOR, irq_handler);

	write_msr(X2APIC_TDCR, 3);

	start = read_pm_timer();
	write_msr(X2APIC_TMICT, 0xffffffff);

	while (read_pm_timer() - start < 100 * NS_PER_MSEC)
		cpu_relax();

	end = read_pm_timer();
	tmr = read_msr(X2APIC_TMCCT);

	apic_frequency = (0xffffffff - tmr) * NS_PER_SEC / (end - start);

	printk("Calibrated APIC frequency: %lu kHz\n",
	       (apic_frequency * 16 + 500) / 1000);

	write_msr(X2APIC_LVTT, APIC_TIMER_VECTOR);
	expected_time = read_pm_timer();
	write_msr(X2APIC_TMICT, 1);

	asm volatile("sti");
}

void inmate_main(void)
{
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
			printk("Rejecting first shutdown request - "
			       "try again!\n");
			jailhouse_send_reply_from_cell(comm_region,
					JAILHOUSE_MSG_REQUEST_DENIED);
			terminate = true;
			break;
		default:
			jailhouse_send_reply_from_cell(comm_region,
					JAILHOUSE_MSG_UNKNOWN);
			break;
		}
	}

	for (n = 0; n < 10; n++)
		asm volatile("hlt");

	printk("Stopped APIC demo\n");
	comm_region->cell_state = JAILHOUSE_CELL_SHUT_DOWN;

	asm volatile("cli; hlt");
}
