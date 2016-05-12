/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_CELL_H
#define _JAILHOUSE_ASM_CELL_H

#ifndef __ASSEMBLY__

#include <jailhouse/paging.h>

struct arch_cell {
	struct paging_structures mm;

	u32 irq_bitmap[1024/32];
};

#endif /* !__ASSEMBLY__ */
#endif /* !_JAILHOUSE_ASM_CELL_H */
