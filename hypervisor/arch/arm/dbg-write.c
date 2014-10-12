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

#include <jailhouse/entry.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/debug.h>
#include <asm/platform.h>

static struct uart_chip uart;

void arch_dbg_write_init(void)
{
	/* FIXME: parse a device tree */
	uart.baudrate = 115200;
	uart.fifo_enabled = true;
	uart.virt_base = hypervisor_header.debug_uart_base;

	uart_chip_init(&uart);
}

void arch_dbg_write(const char *msg)
{
	char c;

	while (1) {
		c = *msg++;
		if (!c)
			break;

		uart.wait(&uart);
		if (panic_in_progress && panic_cpu != phys_processor_id())
			break;
		uart.write(&uart, c);
		uart.busy(&uart);
	}
}
