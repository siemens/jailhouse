/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright 2018 NXP
 *
 * Authors:
 *  Peng Fan <peng.fan@nxp.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/uart.h>

#define UART_DATA		0x1c
#define UART_STAT		0x14
#define STAT_TDRE		(1 << 23)

static void uart_init(struct uart_chip *chip)
{
}

static bool uart_is_busy(struct uart_chip *chip)
{
	return !(mmio_read32(chip->virt_base + UART_STAT) & STAT_TDRE);
}

static void uart_write_char(struct uart_chip *chip, char c)
{
	mmio_write32(chip->virt_base + UART_DATA, c);
}

struct uart_chip uart_imx_lpuart_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write_char = uart_write_char,
};
