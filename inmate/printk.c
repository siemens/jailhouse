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

#include <stdarg.h>
#include <inmate.h>

#ifdef CONFIG_UART_OXPCIE952
#define UART_BASE		0xe010
#else
#define UART_BASE		0x3f8
#endif
#define  UART_TX		0x0
#define  UART_LSR		0x5
#define  UART_LSR_THRE		0x20

static void uart_write(const char *msg)
{
	char c;

	while (1) {
		c = *msg++;
		if (!c)
			break;
		while (!(inb(UART_BASE + UART_LSR) & UART_LSR_THRE))
			cpu_relax();
		outb(c, UART_BASE + UART_TX);
	}
}

#define console_write(msg)	uart_write(msg)
#include "../hypervisor/printk-core.c"

void printk(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	__vprintk(fmt, ap);

	va_end(ap);
}
