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

#ifndef _JAILHOUSE_ASM_ARM64_GIC_V3_H
#define _JAILHOUSE_ASM_ARM64_GIC_V3_H

#include <asm/sysregs.h>

#define ICH_LR0_7_EL2(x)		SYSREG_64(4, c12, c12, x)
#define ICH_LR8_15_EL2(x)		SYSREG_64(4, c12, c13, x)

#define ICC_SGI1R_EL1			SYSREG_64(0, c12, c11, 5)

#define ARM_GIC_READ_LR0_7(n, val)	arm_read_sysreg(ICH_LR0_7_EL2(n), val);
#define ARM_GIC_WRITE_LR0_7(n, val)	arm_write_sysreg(ICH_LR0_7_EL2(n), val);

#define ARM_GIC_READ_LR8_15(n, val)	arm_read_sysreg(ICH_LR8_15_EL2(n), val);
#define ARM_GIC_WRITE_LR8_15(n, val)	\
	arm_write_sysreg(ICH_LR8_15_EL2(n), val);

#endif /* _JAILHOUSE_ASM_ARM64_GIC_V3_H */
