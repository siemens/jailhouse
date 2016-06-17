/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>

extern volatile unsigned long panic_in_progress;
extern unsigned long panic_cpu;

void printk(const char *fmt, ...);

void panic_printk(const char *fmt, ...);

#ifdef CONFIG_TRACE_ERROR
#define trace_error(code) ({						  \
	printk("%s:%d: returning error %s\n", __FILE__, __LINE__, #code); \
	code;								  \
})
#else /* !CONFIG_TRACE_ERROR */
#define trace_error(code)	code
#endif /* !CONFIG_TRACE_ERROR */

void arch_dbg_write_init(void);
void arch_dbg_write(const char *msg);
