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
#ifndef _JAILHOUSE_INMATES_INMATE_H
#define _JAILHOUSE_INMATES_INMATE_H

#ifndef __ASSEMBLY__

static inline void *memset(void *addr, int val, unsigned int size)
{
	char *s = addr;
	unsigned int i;
	for (i = 0; i < size; i++)
		*s++ = val;

	return addr;
}

extern unsigned long printk_uart_base;
void printk(const char *fmt, ...);
void inmate_main(void);

void __attribute__((interrupt("IRQ"))) __attribute__((used)) vector_irq(void);

typedef void (*irq_handler_t)(unsigned int);
void gic_setup(irq_handler_t handler);
void gic_enable_irq(unsigned int irq);

#endif /* !__ASSEMBLY__ */
#endif
