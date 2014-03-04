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
#define JAILHOUSE_HC_CELL_DESTROY		2
#define JAILHOUSE_HC_CELL_GET_STATE		4

#define JAILHOUSE_MSG_NONE			0

/* messages to cell */
#define JAILHOUSE_MSG_SHUTDOWN_REQUESTED	1

/* replies from cell */
#define JAILHOUSE_MSG_SHUTDOWN_DENIED		1
#define JAILHOUSE_MSG_SHUTDOWN_OK		2

/* cell state, initialized by hypervisor, updated by cell */
#define JAILHOUSE_CELL_RUNNING			0
#define JAILHOUSE_CELL_SHUT_DOWN		1 /* terminal state */
#define JAILHOUSE_CELL_FAILED			2 /* terminal state */

struct jailhouse_comm_region {
	volatile __u32 msg_to_cell;
	volatile __u32 reply_from_cell;

	volatile __u32 cell_state;

	/* errors etc. */
};

#include <asm/jailhouse_hypercall.h>

#endif /* !_JAILHOUSE_HYPERCALL_H */
