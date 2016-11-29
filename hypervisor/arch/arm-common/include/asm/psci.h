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

#ifndef _JAILHOUSE_ASM_PSCI_H
#define _JAILHOUSE_ASM_PSCI_H

#define PSCI_VERSION			0x84000000
#define PSCI_CPU_SUSPEND_32		0x84000001
#define PSCI_CPU_SUSPEND_64		0xc4000001
#define PSCI_CPU_OFF			0x84000002
#define PSCI_CPU_ON_32			0x84000003
#define PSCI_CPU_ON_64			0xc4000003
#define PSCI_AFFINITY_INFO_32		0x84000004
#define PSCI_AFFINITY_INFO_64		0xc4000004

/* v0.1 function IDs as used by U-Boot */
#define PSCI_CPU_OFF_V0_1_UBOOT		0x95c1ba5f
#define PSCI_CPU_ON_V0_1_UBOOT		0x95c1ba60

#define PSCI_SUCCESS			0
#define PSCI_NOT_SUPPORTED		(-1)
#define PSCI_INVALID_PARAMETERS		(-2)
#define PSCI_DENIED			(-3)
#define PSCI_ALREADY_ON			(-4)

#define PSCI_CPU_IS_ON			0
#define PSCI_CPU_IS_OFF			1

#define IS_PSCI_32(hvc)			(((hvc) >> 24) == 0x84)
#define IS_PSCI_64(hvc)			(((hvc) >> 24) == 0xc4)
#define IS_PSCI_UBOOT(hvc)		(((hvc) >> 8) == 0x95c1ba)

#define PSCI_INVALID_ADDRESS		(-1L)

struct trap_context;

long psci_dispatch(struct trap_context *ctx);

#endif /* _JAILHOUSE_ASM_PSCI_H */
