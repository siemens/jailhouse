/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2017 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Author:
 *  Lokesh Vutla <lokeshvutla@ti.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_ARM_GIC_V3_H
#define _JAILHOUSE_ASM_ARM_GIC_V3_H

#include <asm/sysregs.h>

#define ICH_LR0_7(x)		SYSREG_32(4, c12, c12, x)
#define ICH_LR8_15(x)		SYSREG_32(4, c12, c13, x)
#define ICH_LRC0_7(x)		SYSREG_32(4, c12, c14, x)
#define ICH_LRC8_15(x)		SYSREG_32(4, c12, c15, x)

#define ICC_SGI1R_EL1		SYSREG_64(0, c12)

#define ARM_GIC_READ_LR0_7(n, val) do {		\
	u32 lr##n, lrc##n;			\
						\
	arm_read_sysreg(ICH_LR0_7(n), lr##n);	\
	arm_read_sysreg(ICH_LRC0_7(n), lrc##n);	\
						\
	val = ((u64)lrc##n << 32) | lr##n;	\
} while (0);

#define ARM_GIC_WRITE_LR0_7(n, val) do {		\
	arm_write_sysreg(ICH_LR0_7(n), (u32)val);	\
	arm_write_sysreg(ICH_LRC0_7(n), val >> 32);	\
} while (0);

#define ARM_GIC_READ_LR8_15(n, val) do {		\
	u32 lr_##n, lrc_##n;				\
							\
	arm_read_sysreg(ICH_LR8_15(n), lr_##n);		\
	arm_read_sysreg(ICH_LRC8_15(n), lrc_##n);	\
							\
	val = ((u64)lrc_##n << 32) | lr_##n;		\
} while (0);

#define ARM_GIC_WRITE_LR8_15(n, val) do {		\
	arm_write_sysreg(ICH_LR8_15(n), (u32)val);	\
	arm_write_sysreg(ICH_LRC8_15(n), val >> 32);	\
} while (0);

#endif /* _JAILHOUSE_ASM_ARM_GIC_V3_H */
