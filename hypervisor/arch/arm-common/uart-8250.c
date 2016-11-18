/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/processor.h>
#include <asm/uart.h>

#define UART_TX			0x0
#define UART_DLL		0x0
#define UART_DLM		0x4
#define UART_LCR		0xc
#define  UART_LCR_8N1		0x03
#define  UART_LCR_DLAB		0x80
#define UART_LSR		0x14
#define  UART_LSR_THRE		0x20

static void uart_init(struct uart_chip *chip)
{
	if (chip->clock_reg)
		mmio_write32(chip->clock_reg,
			     mmio_read32(chip->clock_reg) |
			     (1 << chip->gate_nr));

	mmio_write32(chip->virt_base + UART_LCR, UART_LCR_DLAB);
	mmio_write32(chip->virt_base + UART_DLL, 0x0d);
	mmio_write32(chip->virt_base + UART_DLM, 0);
	mmio_write32(chip->virt_base + UART_LCR, UART_LCR_8N1);
}

static void uart_wait(struct uart_chip *chip)
{
	while (!(mmio_read32(chip->virt_base + UART_LSR) & UART_LSR_THRE))
		cpu_relax();
}

static void uart_write(struct uart_chip *chip, char c)
{
	mmio_write32(chip->virt_base + UART_TX, c);
}

void uart_chip_init(struct uart_chip *chip)
{
	chip->wait = uart_wait;
	chip->write = uart_write;

	uart_init(chip);
}
