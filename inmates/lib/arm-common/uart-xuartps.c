/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>
#include <uart.h>

#define UART_SR			0x2c
#define  UART_SR_TXEMPTY	0x8
#define UART_FIFO		0x30

static void uart_init(struct uart_chip *chip)
{
}

static bool uart_is_busy(struct uart_chip *chip)
{
	return !(mmio_read32(chip->base + UART_SR) & UART_SR_TXEMPTY);
}

static void uart_write(struct uart_chip *chip, char c)
{
	mmio_write32(chip->base + UART_FIFO, c);
}

struct uart_chip uart_xuartps_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write = uart_write,
};
