/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2016
 * Copyright (c) Siemens AG, 2020
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/uart.h>
#include <jailhouse/control.h>
#include <jailhouse/processor.h>

struct uart_chip *uart = NULL;

static void uart_write_char(char c)
{
	while (uart->is_busy(uart))
		cpu_relax();
	if (panic_in_progress && panic_cpu != phys_processor_id())
		return;
	uart->write_char(uart, c);
}

void uart_write(const char *msg)
{
	char c;

	while (1) {
		c = *msg++;
		if (!c)
			break;

		if (c == '\n')
			uart_write_char('\r');

		uart_write_char(c);
	}
}
