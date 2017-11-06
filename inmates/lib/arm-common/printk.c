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
#include <stdarg.h>
#include <uart.h>
#include <mach.h>

#ifndef CON_DIVIDER
#define CON_DIVIDER 0
#endif

#ifndef CON_CLOCK_REG
#define CON_CLOCK_REG 0
#endif

#ifndef CON_GATE_NR
#define CON_GATE_NR 0
#endif

#define UART_IDLE_LOOPS		100

static struct uart_chip *chip = NULL;

static void console_write(const char *msg)
{
	char c = 0;

	while (1) {
		if (c == '\n')
			c = '\r';
		else
			c = *msg++;
		if (!c)
			break;

		while (chip->is_busy(chip))
			cpu_relax();
		chip->write(chip, c);
	}
}

static void console_init(void)
{
	char buf[32];
	const char *type;
	unsigned int n;

	type = cmdline_parse_str("con-type", buf, sizeof(buf), CON_TYPE);
	if (!strcmp(type, "JAILHOUSE"))
		chip = &uart_jailhouse_ops;
	else if (!strcmp(type, "8250"))
		chip = &uart_8250_ops;
	else if (!strcmp(type, "8250-8"))
		chip = &uart_8250_8_ops;
	else if (!strcmp(type, "PL011"))
		chip = &uart_pl011_ops;
	else if (!strcmp(type, "XUARTPS"))
		chip = &uart_xuartps_ops;
	else if (!strcmp(type, "MVEBU"))
		chip = &uart_mvebu_ops;
	else if (!strcmp(type, "HSCIF"))
		chip = &uart_hscif_ops;

	if (!chip)
		return;

	chip->base = (void *)(unsigned long)
		cmdline_parse_int("con-base", CON_BASE);
	chip->divider = cmdline_parse_int("con-divider", CON_DIVIDER);
	chip->gate_nr = cmdline_parse_int("con-gate-nr", CON_GATE_NR);
	chip->clock_reg = (void *)(unsigned long)
		cmdline_parse_int("con-clock-reg", CON_CLOCK_REG);

	chip->init(chip);

	if (chip->divider == 0) {
		/*
		 * We share the UART with the hypervisor. Make sure all
		 * its outputs are done before starting.
		 */
		do {
			for (n = 0; n < UART_IDLE_LOOPS; n++)
				if (chip->is_busy(chip))
					break;
		} while (n < UART_IDLE_LOOPS);
	}
}

#include "../../../hypervisor/printk-core.c"

void printk(const char *fmt, ...)
{
	static bool inited = false;
	va_list ap;

	if (!inited) {
		console_init();
		inited = true;
	}

	if (!chip)
		return;

	va_start(ap, fmt);

	__vprintk(fmt, ap);

	va_end(ap);
}
