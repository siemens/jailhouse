/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_HYPERCALL_H
#define _JAILHOUSE_HYPERCALL_H

#define JAILHOUSE_HC_DISABLE			0
#define JAILHOUSE_HC_CELL_CREATE		1
#define JAILHOUSE_HC_CELL_START			2
#define JAILHOUSE_HC_CELL_DESTROY		4
#define JAILHOUSE_HC_HYPERVISOR_GET_INFO	5
#define JAILHOUSE_HC_CELL_GET_STATE		6
#define JAILHOUSE_HC_CPU_GET_STATE		7

/* Hypervisor information type */
#define JAILHOUSE_INFO_MEM_POOL_SIZE		0
#define JAILHOUSE_INFO_MEM_POOL_USED		1
#define JAILHOUSE_INFO_REMAP_POOL_SIZE		2
#define JAILHOUSE_INFO_REMAP_POOL_USED		3
#define JAILHOUSE_INFO_NUM_CELLS		4

/* CPU state */
#define JAILHOUSE_CPU_RUNNING			0
#define JAILHOUSE_CPU_FAILED			2 /* terminal state */

#define JAILHOUSE_MSG_NONE			0

/* messages to cell */
#define JAILHOUSE_MSG_SHUTDOWN_REQUEST		1
#define JAILHOUSE_MSG_RECONFIG_COMPLETED	2

/* replies from cell */
#define JAILHOUSE_MSG_UNKNOWN			1
#define JAILHOUSE_MSG_REQUEST_DENIED		2
#define JAILHOUSE_MSG_REQUEST_APPROVED		3
#define JAILHOUSE_MSG_RECEIVED			4

/* cell state, initialized by hypervisor, updated by cell */
#define JAILHOUSE_CELL_RUNNING			0
#define JAILHOUSE_CELL_RUNNING_LOCKED		1
#define JAILHOUSE_CELL_SHUT_DOWN		2 /* terminal state */
#define JAILHOUSE_CELL_FAILED			3 /* terminal state */

struct jailhouse_comm_region {
	volatile __u32 msg_to_cell;
	volatile __u32 reply_from_cell;

	volatile __u32 cell_state;

	/* errors etc. */
};

#include <asm/jailhouse_hypercall.h>

#endif /* !_JAILHOUSE_HYPERCALL_H */
