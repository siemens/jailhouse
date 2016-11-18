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
	struct jailhouse_debug_console *debug_console;

	void (*wait)(struct uart_chip *);
	void (*write)(struct uart_chip *, char c);

	void		*clock_reg;
	unsigned int	gate_nr;
};

void uart_chip_init(struct uart_chip *chip);

#endif /* !__ASSEMBLY__ */
#endif /* !JAILHOUSE_ASM_UART_H_ */
