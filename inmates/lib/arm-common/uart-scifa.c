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

#define SCIFA_SCABRR			0x04
#define SCIFA_SCASCR			0x08
#define SCIFA_SCASSR			0x14
#define SCIFA_SCAFCR			0x18
#define SCIFA_SCAFTDR                   0x20

#define SCIFA_SCASCR_RE			0x0010
#define SCIFA_SCASCR_TE			0x0020

#define SCIFA_SCASSR_TDFE		0x0020
#define SCIFA_SCASSR_TEND		0x0040

#define SCIFA_FIFO_SIZE			64
#define SCIFA_TTRG_32BYTES		0

static void uart_scifa_init(struct uart_chip *chip)
{
	u16 scascr;

	if (chip->divider) {
		scascr = mmio_read16(chip->base + SCIFA_SCASCR);
		mmio_write16(chip->base + SCIFA_SCASCR,
			     scascr & ~(SCIFA_SCASCR_TE | SCIFA_SCASCR_RE));
		mmio_write8(chip->base + SCIFA_SCABRR, chip->divider);
		mmio_write16(chip->base + SCIFA_SCAFCR, SCIFA_TTRG_32BYTES);
		mmio_write16(chip->base + SCIFA_SCASCR, scascr | SCIFA_SCASCR_TE);
	}
}

static bool uart_scifa_is_busy(struct uart_chip *chip)
{
	return !(mmio_read16(chip->base + SCIFA_SCASSR) & SCIFA_SCASSR_TDFE);
}

static void uart_scifa_write(struct uart_chip *chip, char c)
{
	mmio_write8(chip->base + SCIFA_SCAFTDR, c);
	mmio_write16(chip->base + SCIFA_SCASSR,
		     mmio_read16(chip->base + SCIFA_SCASSR) &
				 ~(SCIFA_SCASSR_TDFE | SCIFA_SCASSR_TEND));
}

DEFINE_UART(scifa, "SCIFA", JAILHOUSE_CON_TYPE_SCIFA);
