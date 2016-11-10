/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
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
