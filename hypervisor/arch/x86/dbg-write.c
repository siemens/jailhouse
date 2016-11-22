/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <asm/uart.h>
#include <asm/vga.h>

void arch_dbg_write_init(void)
{
	vga_init();
	uart_init();
}

void arch_dbg_write(const char *msg)
{
	if (vga_mem)
		vga_write(msg);
	else if (uart_base)
		uart_write(msg);
}
