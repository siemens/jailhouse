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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inmate.h>
#include <uart.h>

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

static void uart_hscif_init(struct uart_chip *chip)
{
	u16 hsscr;

	if (chip->divider) {
		hsscr = mmio_read16(chip->base + HSCIF_HSSCR);
		mmio_write16(chip->base + HSCIF_HSSCR,
			     hsscr & ~(HSCIF_HSSCR_TE | HSCIF_HSSCR_RE));
		mmio_write8(chip->base + HSCIF_HSBRR, chip->divider);
		mmio_write16(chip->base + HSCIF_HSTTRGR, HSCIF_FIFO_SIZE / 2);
		mmio_write16(chip->base + HSCIF_HSSCR, hsscr | HSCIF_HSSCR_TE);
	}
}

static bool uart_hscif_is_busy(struct uart_chip *chip)
{
	return !(mmio_read16(chip->base + HSCIF_HSFSR) & HSCIF_HSFSR_TDFE);
}

static void uart_hscif_write(struct uart_chip *chip, char c)
{
	mmio_write8(chip->base + HSCIF_HSFTDR, c);
	mmio_write16(chip->base + HSCIF_HSFSR,
		     mmio_read16(chip->base + HSCIF_HSFSR) &
				 ~(HSCIF_HSFSR_TDFE | HSCIF_HSFSR_TEND));
}

DEFINE_UART(hscif, "HSCIF", JAILHOUSE_CON_TYPE_HSCIF);
