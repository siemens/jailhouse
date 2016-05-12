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

#ifndef _JAILHOUSE_ASM_SYSREGS_H
#define _JAILHOUSE_ASM_SYSREGS_H

#ifndef __ASSEMBLY__

#define arm_write_sysreg(sysreg, val) \
	asm volatile ("msr	"#sysreg", %0\n" : : "r"((u64)(val)))

#define arm_read_sysreg(sysreg, val) \
	asm volatile ("mrs	%0, "#sysreg"\n" : "=r"((u64)(val)))

#endif

#endif
