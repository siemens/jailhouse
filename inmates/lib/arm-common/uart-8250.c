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

#include <inmate.h>
#include <uart.h>

#define UART_TX			0x0
#define UART_DLL		0x0
#define UART_DLM		0x4
#define UART_LCR		0xc
#define  UART_LCR_8N1		0x03
#define  UART_LCR_DLAB		0x80
#define UART_LSR		0x14
#define  UART_LSR_THRE		0x20

static void uart_init(struct uart_chip *chip)
{
	if (chip->clock_reg)
		mmio_write32(chip->clock_reg,
			     mmio_read32(chip->clock_reg) |
			     (1 << chip->gate_nr));

	if (chip->divider) {
		mmio_write32(chip->base + UART_LCR, UART_LCR_DLAB);
		mmio_write32(chip->base + UART_DLL, chip->divider);
		mmio_write32(chip->base + UART_DLM, 0);
		mmio_write32(chip->base + UART_LCR, UART_LCR_8N1);
	}
}

static bool uart_is_busy(struct uart_chip *chip)
{
	return !(mmio_read32(chip->base + UART_LSR) & UART_LSR_THRE);
}

static void uart_write(struct uart_chip *chip, char c)
{
	mmio_write32(chip->base + UART_TX, c);
}

struct uart_chip uart_8250_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write = uart_write,
};
