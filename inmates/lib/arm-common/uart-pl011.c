/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>
#include <uart.h>

#define UART_CLK	24000000

#define UARTDR		0x00
#define UARTFR		0x18
#define UARTIBRD	0x24
#define UARTLCR_H	0x2c
#define UARTCR		0x30

#define UARTFR_TXFF	(1 << 5)
#define UARTFR_BUSY	(1 << 3)

#define UARTCR_Out2  	(1 << 13)
#define UARTCR_Out1  	(1 << 12)
#define UARTCR_RXE   	(1 << 9)
#define UARTCR_TXE   	(1 << 8)
#define UARTCR_EN	(1 << 0)

#define UARTLCR_H_WLEN	(3 << 5)

static void uart_init(struct uart_chip *chip)
{
#ifdef CONFIG_MACH_VEXPRESS
	/* 115200 8N1 */
	/* FIXME: Can be improved with an implementation of __aeabi_uidiv */
	u32 bauddiv = UART_CLK / (16 * 115200);

	mmio_write16(chip->base + UARTCR, 0);
	while (mmio_read8(chip->base + UARTFR) & UARTFR_BUSY)
		cpu_relax();

	mmio_write8(chip->base + UARTLCR_H, UARTLCR_H_WLEN);
	mmio_write16(chip->base + UARTIBRD, bauddiv);
	mmio_write16(chip->base + UARTCR, (UARTCR_EN | UARTCR_TXE | UARTCR_RXE |
					   UARTCR_Out1 | UARTCR_Out2));
#endif
}

static bool uart_is_busy(struct uart_chip *chip)
{
	/* FIFO full or busy */
	return (mmio_read32(chip->base + UARTFR) &
		(UARTFR_TXFF | UARTFR_BUSY)) != 0;
}

static void uart_write(struct uart_chip *chip, char c)
{
	mmio_write32(chip->base + UARTDR, c);
}

struct uart_chip uart_pl011_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write = uart_write,
};
