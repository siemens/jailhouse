/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/uart.h>

#define UART_TX			0x0
#define UART_DLL		0x0
#define UART_DLM		0x1
#define UART_LCR		0x3
#define  UART_LCR_8N1		0x03
#define  UART_LCR_DLAB		0x80
#define UART_LSR		0x5
#define  UART_LSR_THRE		0x20

static void reg_out_mmio32(struct uart_chip *chip, unsigned int reg, u32 value)
{
	mmio_write32(chip->virt_base + reg * 4, value);
}

static u32 reg_in_mmio32(struct uart_chip *chip, unsigned int reg)
{
	return mmio_read32(chip->virt_base + reg * 4);
}

static void uart_init(struct uart_chip *chip)
{
	void *clock_reg = (void*)(unsigned long)chip->virt_clock_reg;
	unsigned int gate_nr = chip->debug_console->gate_nr;

	/* clock setting only implemented on ARM via 32-bit MMIO */
	if (clock_reg)
		mmio_write32(clock_reg,
			     mmio_read32(clock_reg) | (1 << gate_nr));

	/* only initialise if divider is not zero */
	if (!chip->debug_console->divider)
		return;

	chip->reg_out(chip, UART_LCR, UART_LCR_DLAB);
	chip->reg_out(chip, UART_DLL, chip->debug_console->divider & 0xff);
	chip->reg_out(chip, UART_DLM,
		      (chip->debug_console->divider >> 8) & 0xff);
	chip->reg_out(chip, UART_LCR, UART_LCR_8N1);
}

static bool uart_is_busy(struct uart_chip *chip)
{
	return !(chip->reg_in(chip, UART_LSR) & UART_LSR_THRE);
}

static void uart_write_char(struct uart_chip *chip, char c)
{
	chip->reg_out(chip, UART_TX, c);
}

struct uart_chip uart_8250_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write_char = uart_write_char,
	.reg_out = reg_out_mmio32,
	.reg_in = reg_in_mmio32,
};
