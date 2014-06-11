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

#ifndef _JAILHOUSE_ASM_DEBUG_PL011_H
#define _JAILHOUSE_ASM_DEBUG_PL011_H

#include <asm/debug.h>
#include <asm/processor.h>

#define UART_CLK	24000000

#define UARTDR		0x00
#define UARTRSR		0x04
#define UARTECR		0x04
#define UARTFR		0x18
#define UARTILPR	0x20
#define UARTIBRD	0x24
#define UARTFBRD	0x28
#define UARTLCR_H	0x2c
#define UARTCR		0x30
#define UARTIFLS	0x34
#define UARTIMSC	0x38
#define UARTRIS		0x3c
#define UARTMIS		0x40
#define UARTICR		0x44
#define UARTDMACR	0x48

#define UARTFR_RXFF	(1 << 6)
#define UARTFR_TXFE	(1 << 7)
#define UARTFR_TXFF	(1 << 5)
#define UARTFR_RXFE	(1 << 4)
#define UARTFR_BUSY	(1 << 3)
#define UARTFR_DCD	(1 << 2)
#define UARTFR_DSR	(1 << 1)
#define UARTFR_CTS	(1 << 0)

#define UARTCR_CTSEn	(1 << 15)
#define UARTCR_RTSEn 	(1 << 14)
#define UARTCR_Out2  	(1 << 13)
#define UARTCR_Out1  	(1 << 12)
#define UARTCR_RTS   	(1 << 11)
#define UARTCR_DTR   	(1 << 10)
#define UARTCR_RXE   	(1 << 9)
#define UARTCR_TXE   	(1 << 8)
#define UARTCR_LBE   	(1 << 7)
#define UARTCR_SIRLP 	(1 << 2)
#define UARTCR_SIREN 	(1 << 1)
#define UARTCR_EN	(1 << 0)

#define UARTLCR_H_SPS	(1 << 7)
#define UARTLCR_H_WLEN	(3 << 5)
#define UARTLCR_H_FEN 	(1 << 4)
#define UARTLCR_H_STP2	(1 << 3)
#define UARTLCR_H_EPS 	(1 << 2)
#define UARTLCR_H_PEN 	(1 << 1)
#define UARTLCR_H_BRK 	(1 << 0)

#ifndef __ASSEMBLY__

#include <jailhouse/mmio.h>

static void uart_init(struct uart_chip *chip)
{
	/* 115200 8N1 */
	/* FIXME: Can be improved with an implementation of __aeabi_uidiv */
	u32 bauddiv = UART_CLK / (16 * 115200);
	void *base = chip->virt_base;

	mmio_write16(base + UARTCR, 0);
	while (mmio_read8(base + UARTFR) & UARTFR_BUSY)
		cpu_relax();

	mmio_write8(base + UARTLCR_H, UARTLCR_H_WLEN);
	mmio_write16(base + UARTIBRD, bauddiv);
	mmio_write16(base + UARTCR, (UARTCR_EN | UARTCR_TXE | UARTCR_RXE |
				     UARTCR_Out1 | UARTCR_Out2));
}

static void uart_wait(struct uart_chip *chip)
{
	u32 flags;

	do {
		flags = mmio_read32(chip->virt_base + UARTFR);
		cpu_relax();
	} while (flags & UARTFR_TXFF); /* FIFO full */
}

static void uart_busy(struct uart_chip *chip)
{
	u32 flags;

	do {
		flags = mmio_read32(chip->virt_base + UARTFR);
		cpu_relax();
	} while (flags & UARTFR_BUSY);
}

static void uart_write(struct uart_chip *chip, char c)
{
	mmio_write32(chip->virt_base + UARTDR, c);
}

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_DEBUG_PL011_H */
