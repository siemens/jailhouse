/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 *
 * Use with tiny-demo config e.g.
 */

#include <inmate.h>

#ifdef CONFIG_SERIAL_OXPCIE952
#define UART_BASE		0xe000
#else
#define UART_BASE		0x2f8
#endif

#define IA32_EFER	0xc0000080

void inmate_main(void)
{
	printk_uart_base = UART_BASE;
	printk("This runs in 32-bit mode (EFER: %x)\n", read_msr(IA32_EFER));
}
