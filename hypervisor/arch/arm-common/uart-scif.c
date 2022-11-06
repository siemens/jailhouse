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
 */

#include <jailhouse/mmio.h>
#include <jailhouse/processor.h>
#include <jailhouse/uart.h>

#define SCIF_SCFTDR		0x0c	/* Transmit FIFO data register */
#define SCIF_SCFSR		0x10	/* Serial status register */
#define SCIF_SCFSR_TDFE		0x20
#define SCIF_SCFSR_TEND		0x40

static void uart_init(struct uart_chip *chip)
{
}

static bool uart_is_busy(struct uart_chip *chip)
{
	return (!((SCIF_SCFSR_TDFE | SCIF_SCFSR_TEND) &
		mmio_read16(chip->virt_base + SCIF_SCFSR)));
}

static void uart_write_char(struct uart_chip *chip, char c)
{
	mmio_write8(chip->virt_base + SCIF_SCFTDR, c);
	mmio_write16(chip->virt_base + SCIF_SCFSR,
		     mmio_read16(chip->virt_base + SCIF_SCFSR) &
		     ~(SCIF_SCFSR_TDFE | SCIF_SCFSR_TEND));
}

struct uart_chip uart_scif_ops = {
	.init = uart_init,
	.is_busy = uart_is_busy,
	.write_char = uart_write_char,
};
