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
#ifndef _JAILHOUSE_INMATES_GIC_H
#define _JAILHOUSE_INMATES_GIC_H

#include <jailhouse/types.h>

#ifndef __ASSEMBLY__

int gic_init(void);
void gic_enable(unsigned int irqn);
void gic_write_eoi(u32 irqn);
u32 gic_read_ack(void);

#endif /* !__ASSEMBLY__ */
#endif
