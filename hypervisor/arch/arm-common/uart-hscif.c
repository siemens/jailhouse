/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) emtrion GmbH, 2017
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

#define HSCIF_HSBRR			0x04
#define HSCIF_HSSCR			0x08
#define HSCIF_HSFTDR			0x0C
#define HSCIF_HSFSR			0x10
#define HSCIF_HSTTRGR			0x58

#define HSCIF_HSSCR_RE			0x0010
#define HSCIF_HSSCR_TE			0x0020

#define HSCIF_HSFSR_TDFE		0x0020
#define HSCIF_HSFSR_TEND		0x0040

#define HSCIF_FIFO_SIZE			128

static void uart_init(struct uart_chip *chip)
{
	u16 hsscr;

	if (chip->debug_console->divider) {
		hsscr = mmio_read16(chip->virt_base + HSCIF_HSSCR);
		mmio_write16(chip->virt_base + HSCIF_HSSCR,
			     hsscr & ~(HSCIF_HSSCR_TE | HSCIF_HSSCR_RE));
		mmio_write8(chip->virt_base + HSCIF_HSBRR,
			    chip->debug_console->divider);
		mmio_write16(chip->virt_base + HSCIF_HSTTRGR,
			     HSCIF_FIFO_SIZE / 2);
		mmio_write16(chip->virt_base + HSCIF_HSSCR,
			     hsscr | HSCIF_HSSCR_TE);
	}
}

static bool uart_is_busy(struct uart_chip *chip)
{
	return !(mmio_read16(chip->virt_base + HSCIF_HSFSR) &
			     HSCIF_HSFSR_TDFE);
}

static void uart_write_char(struct uart_chip *chip, char c)
{
	mmio_write8(chip->virt_base + HSCIF_HSFTDR, c);
	mmio_write16(chip->virt_base + HSCIF_HSFSR,
		     mmio_read16(chip->virt_base + HSCIF_HSFSR) &
				 ~(HSCIF_HSFSR_TDFE | HSCIF_HSFSR_TEND));
}

struct uart_chip uart_hscif_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write_char = uart_write_char,
};
