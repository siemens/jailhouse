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

#define UART_TX			0x0
#define UART_DLL		0x0
#define UART_DLM		0x1
#define UART_LCR		0x3
#define UART_LCR_8N1		0x03
#define UART_LCR_DLAB		0x80
#define UART_LSR		0x5
#define UART_LSR_THRE		0x20

unsigned int printk_uart_base;

static void uart_write(const char *msg)
{
	char c;

	while (1) {
		c = *msg++;
		if (!c)
			break;
		while (!(inb(printk_uart_base + UART_LSR) & UART_LSR_THRE))
			cpu_relax();
		outb(c, printk_uart_base + UART_TX);
	}
}

#define console_write(msg)	uart_write(msg)
#include "../hypervisor/printk-core.c"

void printk(const char *fmt, ...)
{
	static bool inited;
	va_list ap;

	if (!inited) {
		inited = true;
		outb(UART_LCR_DLAB, printk_uart_base + UART_LCR);
#ifdef CONFIG_UART_OXPCIE952
		outb(0x22, printk_uart_base + UART_DLL);
#else
		outb(1, printk_uart_base + UART_DLL);  
#endif
		outb(0, printk_uart_base + UART_DLM);
		outb(UART_LCR_8N1, printk_uart_base + UART_LCR);
	}

	va_start(ap, fmt);

	__vprintk(fmt, ap);

	va_end(ap);
}
