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

#include <jailhouse/types.h>
#include <asm/debug.h>
#include <stdarg.h>
#include <inmates/inmate.h>

static struct uart_chip chip;

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

		chip.wait(&chip);
		chip.write(&chip, c);
		chip.busy(&chip);
	}
}

#include "../../../hypervisor/printk-core.c"

void printk(const char *fmt, ...)
{
	static bool inited = false;
	va_list ap;

	if (!inited) {
		uart_chip_init(&chip);
		inited = true;
	}

	va_start(ap, fmt);

	__vprintk(fmt, ap);

	va_end(ap);
}
