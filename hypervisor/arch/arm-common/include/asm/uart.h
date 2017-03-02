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

#ifndef JAILHOUSE_ASM_UART_H_
#define JAILHOUSE_ASM_UART_H_

#ifndef __ASSEMBLY__

/* Defines the bare minimum for debug writes */
struct uart_chip {
	void		*virt_base;
	void		*virt_clock_reg;
	struct jailhouse_debug_console *debug_console;

	void (*init)(struct uart_chip *chip);
	bool (*is_busy)(struct uart_chip *chip);
	void (*write_char)(struct uart_chip *chip, char c);

	void (*reg_out)(void *address, u32 value);
	u32 (*reg_in)(void *address);
	unsigned int reg_dist;
};

extern struct uart_chip uart_8250_ops, uart_pl011_ops, uart_xuartps_ops;

#endif /* !__ASSEMBLY__ */
#endif /* !JAILHOUSE_ASM_UART_H_ */
