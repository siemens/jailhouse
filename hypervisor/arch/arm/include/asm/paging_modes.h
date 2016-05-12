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
 */

#ifndef __ASSEMBLY__

#include <jailhouse/paging.h>

/* Long-descriptor paging */
extern const struct paging *hv_paging;
extern const struct paging *cell_paging;

#endif /* !__ASSEMBLY__ */
