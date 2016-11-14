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

#define UART_BASE		0x3f8
#define UART_TX			0x0
#define UART_DLL		0x0
#define UART_DLM		0x1
#define UART_LCR		0x3
#define UART_LCR_8N1		0x03
#define UART_LCR_DLAB		0x80
#define UART_LSR		0x5
#define UART_LSR_THRE		0x20

static unsigned int printk_uart_base;

static void uart_write(const char *msg)
{
	char c = 0;

	if (!printk_uart_base)
		return;

	while (1) {
		if (c == '\n')
			c = '\r';
		else
			c = *msg++;
		if (!c)
			break;
		while (!(inb(printk_uart_base + UART_LSR) & UART_LSR_THRE))
			cpu_relax();
		outb(c, printk_uart_base + UART_TX);
	}
}

#define console_write(msg)	uart_write(msg)
#include "../../../hypervisor/printk-core.c"

static void console_init(void)
{
	unsigned int divider;

	printk_uart_base = cmdline_parse_int("con-base", UART_BASE);
	divider = cmdline_parse_int("con-divider", 0);

	if (!printk_uart_base || !divider)
		return;

	outb(UART_LCR_DLAB, printk_uart_base + UART_LCR);
	outb(divider, printk_uart_base + UART_DLL);
	outb(0, printk_uart_base + UART_DLM);
	outb(UART_LCR_8N1, printk_uart_base + UART_LCR);
}

void printk(const char *fmt, ...)
{
	static bool inited;
	va_list ap;

	if (!inited) {
		console_init();
		inited = true;
	}

	va_start(ap, fmt);

	__vprintk(fmt, ap);

	va_end(ap);
}
