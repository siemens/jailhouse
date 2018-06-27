/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) OTH Regensburg, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/uart.h>
#include <asm/uart.h>

void arch_dbg_write_init(void)
{
	unsigned char con_type = system_config->debug_console.type;

	if (!CON_IS_MMIO(system_config->debug_console.flags))
		return;

	if (con_type == JAILHOUSE_CON_TYPE_PL011)
		uart = &uart_pl011_ops;
	else if (con_type == JAILHOUSE_CON_TYPE_8250)
		uart = &uart_8250_ops;
	else if (con_type == JAILHOUSE_CON_TYPE_XUARTPS)
		uart = &uart_xuartps_ops;
	else if (con_type == JAILHOUSE_CON_TYPE_MVEBU)
		uart = &uart_mvebu_ops;
	else if (con_type == JAILHOUSE_CON_TYPE_HSCIF)
		uart = &uart_hscif_ops;
	else if (con_type == JAILHOUSE_CON_TYPE_SCIFA)
		uart = &uart_scifa_ops;
	else if (con_type == JAILHOUSE_CON_TYPE_IMX)
		uart = &uart_imx_ops;

	if (uart) {
		uart->debug_console = &system_config->debug_console;
		uart->virt_base = hypervisor_header.debug_console_base;
		uart->init(uart);
		arch_dbg_write = uart_write;
	}
}
