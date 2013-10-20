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

#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/io.h>

#ifdef CONFIG_UART_OXPCIE952
#define UART_BASE		0xe010
#else
#define UART_BASE		0x3f8
#endif
#define  UART_TX		0x0
#define  UART_DLL		0x0
#define  UART_DLM		0x1
#define  UART_LCR		0x3
#define  UART_LCR_8N1		0x03
#define  UART_LCR_DLAB		0x80
#define  UART_LSR		0x5
#define  UART_LSR_THRE		0x20

void arch_dbg_write_init(void)
{
	outb(UART_LCR_DLAB, UART_BASE + UART_LCR);
#ifdef CONFIG_UART_OXPCIE952
	outb(0x22, UART_BASE + UART_DLL);
#else
	outb(1, UART_BASE + UART_DLL);
#endif
	outb(0, UART_BASE + UART_DLM);
	outb(UART_LCR_8N1, UART_BASE + UART_LCR);
}

void arch_dbg_write(const char *msg)
{
	char c;

	while (1) {
		c = *msg++;
		if (!c)
			break;
		while (!(inb(UART_BASE + UART_LSR) & UART_LSR_THRE))
			cpu_relax();
		if (panic_in_progress && panic_cpu != phys_processor_id())
			break;
		outb(c, UART_BASE + UART_TX);
	}
}
