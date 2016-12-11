/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>
#include <uart.h>

static void jailhouse_init(struct uart_chip *chip)
{
}

static bool jailhouse_is_busy(struct uart_chip *chip)
{
	return false;
}

static void jailhouse_write(struct uart_chip *chip, char c)
{
	jailhouse_call_arg1(JAILHOUSE_HC_DEBUG_CONSOLE_PUTC, c);
}

struct uart_chip uart_jailhouse_ops = {
	.init = jailhouse_init,
	.is_busy = jailhouse_is_busy,
	.write = jailhouse_write,
};
