/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) OTH Regensburg, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/entry.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/uart.h>

extern struct uart_chip uart_ops;

static struct uart_chip *uart = NULL;

static void arm_uart_write(const char *msg)
{
	char c = 0;

	while (1) {
		if (c == '\n')
			c = '\r';
		else
			c = *msg++;
		if (!c)
			break;

		uart->wait(uart);
		if (panic_in_progress && panic_cpu != phys_processor_id())
			break;
		uart->write(uart, c);
	}
}

void arch_dbg_write_init(void)
{
	unsigned char con_type = CON_TYPE(system_config->debug_console.flags);

	if (!CON_IS_MMIO(system_config->debug_console.flags))
		return;

	if (con_type != JAILHOUSE_CON_TYPE_UART_ARM)
		return;

	uart = &uart_ops;

	if (uart) {
		uart->debug_console = &system_config->debug_console;
		uart->virt_clock_reg = hypervisor_header.debug_clock_reg;
		uart->virt_base = hypervisor_header.debug_console_base;
		uart->init(uart);
		arch_dbg_write = arm_uart_write;
	}
}
