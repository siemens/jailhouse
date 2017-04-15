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

static void reg_out_pio(struct uart_chip *chip, unsigned int reg, u32 value)
{
	outb(value, (u16)(unsigned long long)chip->virt_base + reg);
}

static u32 reg_in_pio(struct uart_chip *chip, unsigned int reg)
{
	return inb((u16)(unsigned long long)chip->virt_base + reg);
}

static void reg_out_mmio8(struct uart_chip *chip, unsigned int reg, u32 value)
{
	mmio_write8(chip->virt_base + reg, value);
}

static u32 reg_in_mmio8(struct uart_chip *chip, unsigned int reg)
{
	return mmio_read8(chip->virt_base + reg);
}

void arch_dbg_write_init(void)
{
	const u32 flags = system_config->debug_console.flags;
	unsigned char dbg_type = CON1_TYPE(flags);

	if (dbg_type == JAILHOUSE_CON1_TYPE_8250) {
		uart = &uart_8250_ops;

		uart->debug_console = &system_config->debug_console;
		if (CON1_IS_MMIO(flags)) {
			uart->virt_base = hypervisor_header.debug_console_base;
			if (CON1_USES_REGDIST_1(flags)) {
				uart->reg_out = reg_out_mmio8;
				uart->reg_in = reg_in_mmio8;
			}
		} else {
			uart->virt_base =
				(void *)system_config->debug_console.address;
			uart->reg_out = reg_out_pio;
			uart->reg_in = reg_in_pio;
		}
		uart->init(uart);
		arch_dbg_write = uart_write;
	} else if (dbg_type == JAILHOUSE_CON1_TYPE_VGA) {
		vga_init();
		arch_dbg_write = vga_write;
	}
}
