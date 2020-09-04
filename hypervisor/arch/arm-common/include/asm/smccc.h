/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (C) 2018 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Authors:
 *  Lokesh Vutla <lokeshvutla@ti.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/types.h>

#define SMCCC_VERSION			0x80000000
#define SMCCC_ARCH_FEATURES		0x80000001
#define SMCCC_ARCH_WORKAROUND_1		0x80008000
#define SMCCC_ARCH_WORKAROUND_2		0x80007fff

#define SDEI_VERSION			0xc4000020
#define SDEI_EVENT_REGISTER		0xc4000021
#define SDEI_EVENT_ENABLE		0xc4000022
#define SDEI_EVENT_COMPLETE		0xc4000025
#define SDEI_EVENT_UNREGISTER		0xc4000027
#define SDEI_PE_MASK			0xc400002b
#define SDEI_PE_UNMASK			0xc400002c
#define SDEI_EVENT_SIGNAL		0xc400002f

#define ARM_SMCCC_VERSION_1_0		0x1000000000000L

#define SDEI_EV_HANDLED			0

#define ARM_SMCCC_OWNER_MASK		BIT_MASK(29, 24)
#define ARM_SMCCC_OWNER_SHIFT		24

#define ARM_SMCCC_OWNER_ARCH		0
#define ARM_SMCCC_OWNER_SIP             2
#define ARM_SMCCC_OWNER_STANDARD        4

#define ARM_SMCCC_CONV_32		0
#define ARM_SMCCC_CONV_64		1

#define ARM_SMCCC_NOT_SUPPORTED         (-1)
#define ARM_SMCCC_SUCCESS               0

#define ARM_SMCCC_VERSION_1_1		0x10001

#define SMCCC_GET_OWNER(id)	((id & ARM_SMCCC_OWNER_MASK) >>	\
				 ARM_SMCCC_OWNER_SHIFT)

#define SMCCC_IS_CONV_64(function_id)	!!(function_id & (1 << 30))

#ifndef __ASSEMBLY__

struct trap_context;

extern bool sdei_available;

int smccc_discover(void);
enum trap_return handle_smc(struct trap_context *ctx);

#endif /* !__ASSEMBLY__ */
