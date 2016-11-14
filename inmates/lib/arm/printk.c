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

#include <inmate.h>
#include <stdarg.h>
#include <uart.h>
#include <mach/uart.h>

static struct uart_chip *chip = NULL;

extern struct uart_chip uart_8250_ops, uart_pl011_ops;

static void console_write(const char *msg)
{
	char c = 0;

	while (1) {
		if (c == '\n')
			c = '\r';
		else
			c = *msg++;
		if (!c)
			break;

		chip->wait(chip);
		chip->write(chip, c);
	}
}

static void console_init(void)
{
	char buf[32];
	const char *type;

	type = cmdline_parse_str("con-type", buf, sizeof(buf), "none");
	if (!strncmp(type, "8250", 4))
		chip = &uart_8250_ops;
	else if (!strncmp(type, "PL011", 5))
		chip = &uart_pl011_ops;

	if (!chip)
		return;

	chip->base = (void *)(unsigned long) cmdline_parse_int("con-base", 0);
	chip->divider = cmdline_parse_int("con-divider", 0);
	chip->gate_nr = cmdline_parse_int("con-gate_nr", 0);
	chip->clock_reg = (void *)(unsigned long)
		cmdline_parse_int("con-clock_reg", 0);

	chip->init(chip);
}

#include "../../../hypervisor/printk-core.c"

void printk(const char *fmt, ...)
{
	static bool inited = false;
	va_list ap;

	if (!inited) {
		console_init();
		inited = true;
	}

	if (!chip)
		return;

	va_start(ap, fmt);

	__vprintk(fmt, ap);

	va_end(ap);
}
