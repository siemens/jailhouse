/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 * Copyright (c) OTH Regensburg, 2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/uart.h>
#include <asm/io.h>
#include <asm/vga.h>

static void uart_pio_out(void *address, u32 value)
{
	outb(value, (u16)(unsigned long long)address);
}

static u32 uart_pio_in(void *address)
{
	return inb((u16)(unsigned long long)address);
}

void arch_dbg_write_init(void)
{
	const u32 flags = system_config->debug_console.flags;
	unsigned char dbg_type = CON1_TYPE(flags);

	if (dbg_type == JAILHOUSE_CON1_TYPE_UART_X86) {
		uart = &uart_8250_ops;

		uart->debug_console = &system_config->debug_console;
		if (CON1_IS_MMIO(flags)) {
			uart->virt_base = hypervisor_header.debug_console_base;
		} else {
			uart->virt_base = (void*)system_config->debug_console.address;
			uart->reg_out = uart_pio_out;
			uart->reg_in = uart_pio_in;
			uart->reg_dist = 1;
		}
		uart->init(uart);
		arch_dbg_write = uart_write;
	} else if (dbg_type == JAILHOUSE_CON1_TYPE_VGA) {
		vga_init();
		arch_dbg_write = vga_write;
	}
}
