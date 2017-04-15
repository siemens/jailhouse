/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2017
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

struct uart_chip {
	/* must be set by the caller */
	void *virt_base;
	void *virt_clock_reg;
	struct jailhouse_debug_console *debug_console;

	/* driver selects defaults, if used */
	void (*reg_out)(struct uart_chip *chip, unsigned int reg, u32 value);
	u32 (*reg_in)(struct uart_chip *chip, unsigned int reg);

	/* set by the driver */
	void (*init)(struct uart_chip *chip);
	bool (*is_busy)(struct uart_chip *chip);
	void (*write_char)(struct uart_chip *chip, char c);
};

void uart_write(const char *msg);

extern struct uart_chip *uart;
extern struct uart_chip uart_8250_ops;
