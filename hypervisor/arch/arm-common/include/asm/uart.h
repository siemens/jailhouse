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

	void (*init)(struct uart_chip*);
	void (*wait)(struct uart_chip *);
	void (*write)(struct uart_chip *, char c);
};

#endif /* !__ASSEMBLY__ */
#endif /* !JAILHOUSE_ASM_UART_H_ */
