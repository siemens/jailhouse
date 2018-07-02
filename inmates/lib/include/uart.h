/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2016-2018
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
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

struct uart_chip {
	const char *name;
	const __u16 type;

	void *base;
	unsigned int divider;

	void (*reg_out)(struct uart_chip *chip, unsigned int reg, u32 value);
	u32 (*reg_in)(struct uart_chip *chip, unsigned int reg);

	void (*init)(struct uart_chip*);
	bool (*is_busy)(struct uart_chip*);
	void (*write)(struct uart_chip*, char c);
};

extern struct uart_chip *uart_array[];

#define UART_OPS_NAME(__name) \
	uart_##__name##_ops

#define DECLARE_UART(__name) \
	extern struct uart_chip UART_OPS_NAME(__name)

#define DEFINE_UART_REG(__name, __description, __type, __reg_out, __reg_in) \
	struct uart_chip UART_OPS_NAME(__name) = { \
		.name = __description, \
		.type = __type, \
		.init = uart_##__name##_init, \
		.is_busy = uart_##__name##_is_busy, \
		.write = uart_##__name##_write, \
		.reg_out = __reg_out, \
		.reg_in = __reg_in, \
	}

#define DEFINE_UART(__name, __description, __type) \
	DEFINE_UART_REG(__name, __description, __type, NULL, NULL)

void arch_console_init(struct uart_chip *chip);
