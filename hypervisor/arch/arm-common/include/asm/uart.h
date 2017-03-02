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

extern struct uart_chip uart_pl011_ops, uart_xuartps_ops;

#endif /* !__ASSEMBLY__ */
#endif /* !JAILHOUSE_ASM_UART_H_ */
