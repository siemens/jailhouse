/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2017
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

#include "../arm-common/asm/jailhouse_hypercall.h"

#define JAILHOUSE_CALL_INS		".arch_extension virt\n\t" \
					"hvc #0x4a48"
#define JAILHOUSE_CALL_NUM_RESULT	"r0"
#define JAILHOUSE_CALL_ARG1		"r1"
#define JAILHOUSE_CALL_ARG2		"r2"

/* CPU statistics, arm-specific part */
#define JAILHOUSE_CPU_STAT_VMEXITS_CP15		JAILHOUSE_GENERIC_CPU_STATS + 5
#define JAILHOUSE_NUM_CPU_STATS			JAILHOUSE_GENERIC_CPU_STATS + 6

#ifndef __ASSEMBLY__

static inline __u32 jailhouse_call(__u32 num)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;

	asm volatile(
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result)
		: "memory");
	return num_result;
}

static inline __u32 jailhouse_call_arg1(__u32 num, __u32 arg1)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __u32 __arg1 asm(JAILHOUSE_CALL_ARG1) = arg1;

	asm volatile(
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result), "r" (__arg1)
		: "memory");
	return num_result;
}

static inline __u32 jailhouse_call_arg2(__u32 num, __u32 arg1, __u32 arg2)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __u32 __arg1 asm(JAILHOUSE_CALL_ARG1) = arg1;
	register __u32 __arg2 asm(JAILHOUSE_CALL_ARG2) = arg2;

	asm volatile(
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result), "r" (__arg1), "r" (__arg2)
		: "memory");
	return num_result;
}

static inline void
jailhouse_send_msg_to_cell(struct jailhouse_comm_region *comm_region,
			   __u32 msg)
{
	comm_region->reply_from_cell = JAILHOUSE_MSG_NONE;
	/* ensure reply was cleared before sending new message */
	asm volatile("dmb ishst" : : : "memory");
	comm_region->msg_to_cell = msg;
}

static inline void
jailhouse_send_reply_from_cell(struct jailhouse_comm_region *comm_region,
			       __u32 reply)
{
	comm_region->msg_to_cell = JAILHOUSE_MSG_NONE;
	/* ensure message was cleared before sending reply */
	asm volatile("dmb ishst" : : : "memory");
	comm_region->reply_from_cell = reply;
}

#endif /* !__ASSEMBLY__ */
