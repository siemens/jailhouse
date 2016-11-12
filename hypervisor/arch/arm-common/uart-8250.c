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
	void *clock_reg = (void*)(unsigned long)chip->virt_clock_reg;
	unsigned int gate_nr = chip->debug_console->gate_nr;

	if (clock_reg)
		mmio_write32(clock_reg,
			     mmio_read32(clock_reg) | (1 << gate_nr));

	/* only initialise if divider is not zero */
	if (!chip->debug_console->divider)
		return;

	mmio_write32(chip->virt_base + UART_LCR, UART_LCR_DLAB);
	mmio_write32(chip->virt_base + UART_DLL, chip->debug_console->divider);
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

struct uart_chip uart_ops = {
	.wait = uart_wait,
	.write = uart_write,
	.init = uart_init,
};
