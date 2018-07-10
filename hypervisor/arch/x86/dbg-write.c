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
#include <asm/efifb.h>
#include <asm/io.h>

static void reg_out_pio(struct uart_chip *chip, unsigned int reg, u32 value)
{
	outb(value, (u16)(unsigned long long)chip->virt_base + reg);
}

static u32 reg_in_pio(struct uart_chip *chip, unsigned int reg)
{
	return inb((u16)(unsigned long long)chip->virt_base + reg);
}

void arch_dbg_write_init(void)
{
	u32 dbg_type = system_config->debug_console.type;

	if (dbg_type == JAILHOUSE_CON_TYPE_8250) {
		uart = &uart_8250_ops;

		uart->debug_console = &system_config->debug_console;
		if (CON_IS_MMIO(system_config->debug_console.flags)) {
			uart->virt_base = hypervisor_header.debug_console_base;
		} else {
			uart->virt_base =
				(void *)system_config->debug_console.address;
			uart->reg_out = reg_out_pio;
			uart->reg_in = reg_in_pio;
		}
		uart->init(uart);
		arch_dbg_write = uart_write;
	} else if (dbg_type == JAILHOUSE_CON_TYPE_EFIFB) {
		efifb_init();
		arch_dbg_write = efifb_write;
	}
}
