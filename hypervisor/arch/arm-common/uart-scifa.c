/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) emtrion GmbH, 2018
 *
 * Authors:
 *  Ruediger Fichter <ruediger.fichter@emtrion.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/processor.h>
#include <jailhouse/uart.h>

#define SCIFA_SCABRR			0x04
#define SCIFA_SCASCR			0x08
#define SCIFA_SCASSR			0x14
#define SCIFA_SCAFCR			0x18
#define SCIFA_SCAFTDR			0x20

#define SCIFA_SCASCR_RE			0x0010
#define SCIFA_SCASCR_TE			0x0020

#define SCIFA_SCASSR_TDFE		0x0020
#define SCIFA_SCASSR_TEND		0x0040

#define SCIFA_FIFO_SIZE			64
#define SCIFA_TTRG_32BYTES		0

static void uart_init(struct uart_chip *chip)
{
	u16 scascr;

	if (chip->debug_console->divider) {
		scascr = mmio_read16(chip->virt_base + SCIFA_SCASCR);
		mmio_write16(chip->virt_base + SCIFA_SCASCR,
			     scascr & ~(SCIFA_SCASCR_TE | SCIFA_SCASCR_RE));
		mmio_write8(chip->virt_base + SCIFA_SCABRR,
			    chip->debug_console->divider);
		mmio_write16(chip->virt_base + SCIFA_SCAFCR,
			     SCIFA_TTRG_32BYTES);
		mmio_write16(chip->virt_base + SCIFA_SCASCR,
			     scascr | SCIFA_SCASCR_TE);
	}
}

static bool uart_is_busy(struct uart_chip *chip)
{
	return !(mmio_read16(chip->virt_base + SCIFA_SCASSR) &
			     SCIFA_SCASSR_TDFE);
}

static void uart_write_char(struct uart_chip *chip, char c)
{
	mmio_write8(chip->virt_base + SCIFA_SCAFTDR, c);
	mmio_write16(chip->virt_base + SCIFA_SCASSR,
		     mmio_read16(chip->virt_base + SCIFA_SCASSR) &
				 ~(SCIFA_SCASSR_TDFE | SCIFA_SCASSR_TEND));
}

struct uart_chip uart_scifa_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write_char = uart_write_char,
};
