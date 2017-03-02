/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2016
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>
#include <jailhouse/uart.h>
#include <jailhouse/control.h>
#include <jailhouse/processor.h>

struct uart_chip *uart = NULL;

void uart_write(const char *msg)
{
	char c = 0;

	while (1) {
		if (c == '\n')
			c = '\r';
		else
			c = *msg++;
		if (!c)
			break;

		while (uart->is_busy(uart))
			cpu_relax();
		if (panic_in_progress && panic_cpu != phys_processor_id())
			break;
		uart->write_char(uart, c);
	}
}
