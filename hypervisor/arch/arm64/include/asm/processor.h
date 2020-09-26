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

#ifndef _JAILHOUSE_ASM_PROCESSOR_H
#define _JAILHOUSE_ASM_PROCESSOR_H

#include <jailhouse/types.h>

/* also include from arm-common */
#include_next <asm/processor.h>

#define NUM_USR_REGS		31

#define ARM_PARKING_CODE		\
	0xd503207f, /* 1: wfi  */	\
	0x17ffffff, /*    b 1b */

#ifndef __ASSEMBLY__

union registers {
	struct {
		/*
		 * We have an odd number of registers, and the stack needs to
		 * be aligned after pushing all registers. Add 64 bit padding
		 * at the beginning.
		 */
		unsigned long __padding;
		unsigned long usr[NUM_USR_REGS];
	};
};

#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PROCESSOR_H */
