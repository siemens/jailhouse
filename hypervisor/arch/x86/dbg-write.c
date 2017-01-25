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
	unsigned char dbg_type = CON1_TYPE(system_config->debug_console.flags);

	/* PIO / MMIO differentiation is done inside the driver code */
	if (dbg_type == JAILHOUSE_CON1_TYPE_UART_X86) {
		uart_init();
		arch_dbg_write = uart_write;
	} else if (dbg_type == JAILHOUSE_CON1_TYPE_VGA) {
		vga_init();
		arch_dbg_write = vga_write;
	}
}
