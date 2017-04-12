/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (C) Siemens AG, 2017
 *
 * Authors:
 *  Henning Schild <henning.schild@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifdef __ASSEMBLY__
#define __MAKE_UL(x)	x
#else /* !__ASSEMBLY__ */
#define __MAKE_UL(x)	x ## UL
#endif

#define JAILHOUSE_BASE			__MAKE_UL(0xfffffffff0000000)
