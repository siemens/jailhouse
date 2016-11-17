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
#include <mach/console.h>

#ifndef CON_TYPE
#define CON_TYPE "none"
#endif

#ifndef CON_BASE
#define CON_BASE 0
#endif

#ifndef CON_DIVIDER
#define CON_DIVIDER 0
#endif

#ifndef CON_CLOCK_REG
#define CON_CLOCK_REG 0
#endif

#ifndef CON_GATE_NR
#define CON_GATE_NR 0
#endif

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

	type = cmdline_parse_str("con-type", buf, sizeof(buf), CON_TYPE);
	if (!strncmp(type, "8250", 4))
		chip = &uart_8250_ops;
	else if (!strncmp(type, "PL011", 5))
		chip = &uart_pl011_ops;

	if (!chip)
		return;

	chip->base = (void *)(unsigned long)
		cmdline_parse_int("con-base", CON_BASE);
	chip->divider = cmdline_parse_int("con-divider", CON_DIVIDER);
	chip->gate_nr = cmdline_parse_int("con-gate_nr", CON_GATE_NR);
	chip->clock_reg = (void *)(unsigned long)
		cmdline_parse_int("con-clock_reg", CON_CLOCK_REG);

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
