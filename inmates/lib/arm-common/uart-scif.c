/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (C) 2022 Renesas Electronics Corp.
 *
 * Authors:
 *  Lad Prabhakar <prabhakar.mahadev-lad.rj@bp.renesas.com>
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

#define SCIF_SCFTDR		0x0c	/* Transmit FIFO data register */
#define SCIF_SCFSR		0x10	/* Serial status register */
#define SCIF_SCFSR_TDFE		0x20
#define SCIF_SCFSR_TEND		0x40

static void uart_scif_init(struct uart_chip *chip)
{
}

static bool uart_scif_is_busy(struct uart_chip *chip)
{
	return (!((SCIF_SCFSR_TDFE | SCIF_SCFSR_TEND) &
		mmio_read16(chip->base + SCIF_SCFSR)));
}

static void uart_scif_write(struct uart_chip *chip, char c)
{
	mmio_write8(chip->base + SCIF_SCFTDR, c);
	mmio_write16(chip->base + SCIF_SCFSR,
		     mmio_read16(chip->base + SCIF_SCFSR) &
		     ~(SCIF_SCFSR_TDFE | SCIF_SCFSR_TEND));
}

DEFINE_UART(scif, "SCIF", JAILHOUSE_CON_TYPE_SCIF);
