/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright 2017 NXP
 *
 * Authors:
 *  Peng Fan <peng.fan@nxp.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/uart.h>

#define UTS			0xb4
#define UTXD			0x40
#define UTS_TXEMPTY		(1 << 6)

static void uart_init(struct uart_chip *chip)
{
	/* Initialization currently done by Linux */
}

static bool uart_is_busy(struct uart_chip *chip)
{
	return !(mmio_read32(chip->virt_base + UTS) & UTS_TXEMPTY);
}

static void uart_write_char(struct uart_chip *chip, char c)
{
	mmio_write32(chip->virt_base + UTXD, c);
}

struct uart_chip uart_imx_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write_char = uart_write_char,
};
