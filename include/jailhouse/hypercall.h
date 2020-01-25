/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _JAILHOUSE_HYPERCALL_H
#define _JAILHOUSE_HYPERCALL_H

#include <jailhouse/console.h>

#define JAILHOUSE_HC_DISABLE			0
#define JAILHOUSE_HC_CELL_CREATE		1
#define JAILHOUSE_HC_CELL_START			2
#define JAILHOUSE_HC_CELL_SET_LOADABLE		3
#define JAILHOUSE_HC_CELL_DESTROY		4
#define JAILHOUSE_HC_HYPERVISOR_GET_INFO	5
#define JAILHOUSE_HC_CELL_GET_STATE		6
#define JAILHOUSE_HC_CPU_GET_INFO		7
#define JAILHOUSE_HC_DEBUG_CONSOLE_PUTC		8

/* Hypervisor information type */
#define JAILHOUSE_INFO_MEM_POOL_SIZE		0
#define JAILHOUSE_INFO_MEM_POOL_USED		1
#define JAILHOUSE_INFO_REMAP_POOL_SIZE		2
#define JAILHOUSE_INFO_REMAP_POOL_USED		3
#define JAILHOUSE_INFO_NUM_CELLS		4

/* Hypervisor information type */
#define JAILHOUSE_CPU_INFO_STATE		0
#define JAILHOUSE_CPU_INFO_STAT_BASE		1000

/* CPU state */
#define JAILHOUSE_CPU_RUNNING			0
#define JAILHOUSE_CPU_FAILED			2 /* terminal state */

/* CPU statistics */
#define JAILHOUSE_CPU_STAT_VMEXITS_TOTAL	0
#define JAILHOUSE_CPU_STAT_VMEXITS_MMIO		1
#define JAILHOUSE_CPU_STAT_VMEXITS_MANAGEMENT	2
#define JAILHOUSE_CPU_STAT_VMEXITS_HYPERCALL	3
#define JAILHOUSE_GENERIC_CPU_STATS		4

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
#define JAILHOUSE_CELL_FAILED_COMM_REV		4 /* terminal state */

/* indicates if inmate may use the Debug Console putc hypercall */
#define JAILHOUSE_COMM_FLAG_DBG_PUTC_PERMITTED	0x0001
/* indicates if inmate shall use Debug Console putc as output channel */
#define JAILHOUSE_COMM_FLAG_DBG_PUTC_ACTIVE	0x0002

#define JAILHOUSE_COMM_HAS_DBG_PUTC_PERMITTED(flags) \
	!!((flags) & JAILHOUSE_COMM_FLAG_DBG_PUTC_PERMITTED)
#define JAILHOUSE_COMM_HAS_DBG_PUTC_ACTIVE(flags) \
	!!((flags) & JAILHOUSE_COMM_FLAG_DBG_PUTC_ACTIVE)

#define COMM_REGION_ABI_REVISION		2
#define COMM_REGION_MAGIC			"JHCOMM"

#define COMM_REGION_GENERIC_HEADER					\
	/** Communication region magic JHCOMM */			\
	char signature[6];						\
	/** Communication region ABI revision */			\
	__u16 revision;							\
	/** Cell state, initialized by hypervisor, updated by cell. */	\
	volatile __u32 cell_state;					\
	/** Message code sent from hypervisor to cell. */		\
	volatile __u32 msg_to_cell;					\
	/** Reply code sent from cell to hypervisor. */			\
	volatile __u32 reply_from_cell;					\
	/** Holds static flags, see JAILHOUSE_COMM_FLAG_*. */		\
	__u32 flags;							\
	/** Debug console that may be accessed by the inmate. */	\
	struct jailhouse_console console;				\
	/** Base address of PCI memory mapped config. */		\
	__u64 pci_mmconfig_base;

#include <asm/jailhouse_hypercall.h>

#endif /* !_JAILHOUSE_HYPERCALL_H */
