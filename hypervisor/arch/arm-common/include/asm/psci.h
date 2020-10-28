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

/* PSCI v0.2 interface */
#define PSCI_0_2_FN(n)			(0x84000000 + (n))
#define PSCI_0_2_FN64(n)		(0xc4000000 + (n))

#define PSCI_0_2_FN_VERSION		PSCI_0_2_FN(0)
#define PSCI_0_2_FN_CPU_SUSPEND		PSCI_0_2_FN(1)
#define PSCI_0_2_FN_CPU_OFF		PSCI_0_2_FN(2)
#define PSCI_0_2_FN_CPU_ON		PSCI_0_2_FN(3)
#define PSCI_0_2_FN_AFFINITY_INFO	PSCI_0_2_FN(4)

#define PSCI_0_2_FN64_CPU_SUSPEND	PSCI_0_2_FN64(1)
#define PSCI_0_2_FN64_CPU_ON		PSCI_0_2_FN64(3)
#define PSCI_0_2_FN64_AFFINITY_INFO	PSCI_0_2_FN64(4)

/* PSCI v1.0 interface */
#define PSCI_1_0_FN_FEATURES		PSCI_0_2_FN(10)

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

#define IS_PSCI_UBOOT(hvc)		(((hvc) >> 8) == 0x95c1ba)

#define PSCI_INVALID_ADDRESS		~(0UL)

#define PSCI_VERSION_MAJOR(ver)		(u16)((ver) >> 16)
#define PSCI_VERSION(major, minor)	(((major) << 16) | (minor))

struct trap_context;

long psci_dispatch(struct trap_context *ctx);
