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

#include <asm/uart-pl011.h>

void uart_chip_init(struct uart_chip *chip)
{
	chip->wait = uart_wait;
	chip->write = uart_write;

	uart_init(chip);
}
