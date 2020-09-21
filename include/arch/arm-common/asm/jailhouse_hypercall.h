/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2018
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
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

#define JAILHOUSE_HVC_CODE		0x4a48

/* CPU statistics, common part */
#define JAILHOUSE_CPU_STAT_VMEXITS_MAINTENANCE	JAILHOUSE_GENERIC_CPU_STATS
#define JAILHOUSE_CPU_STAT_VMEXITS_VIRQ		JAILHOUSE_GENERIC_CPU_STATS + 1
#define JAILHOUSE_CPU_STAT_VMEXITS_VSGI		JAILHOUSE_GENERIC_CPU_STATS + 2
#define JAILHOUSE_CPU_STAT_VMEXITS_PSCI		JAILHOUSE_GENERIC_CPU_STATS + 3
#define JAILHOUSE_CPU_STAT_VMEXITS_SMCCC	JAILHOUSE_GENERIC_CPU_STATS + 4

#ifndef __ASSEMBLY__

struct jailhouse_comm_region {
	COMM_REGION_GENERIC_HEADER;
	__u8 gic_version;
	__u8 padding[7];
	__u64 gicd_base;
	__u64 gicc_base;
	__u64 gicr_base;
	__u32 vpci_irq_base;
} __attribute__((packed));

static inline __jh_arg jailhouse_call(__jh_arg num)
{
	register __jh_arg num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;

	asm volatile(
		JAILHOUSE_CALL_INS
		: "+r" (num_result)
		: : "memory", JAILHOUSE_CALL_ARG1, JAILHOUSE_CALL_ARG2,
		    JAILHOUSE_CALL_CLOBBERED);
	return num_result;
}

static inline __jh_arg jailhouse_call_arg1(__jh_arg num, __jh_arg arg1)
{
	register __jh_arg num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __jh_arg __arg1 asm(JAILHOUSE_CALL_ARG1) = arg1;

	asm volatile(
		JAILHOUSE_CALL_INS
		: "+r" (num_result), "+r" (__arg1)
		: : "memory", JAILHOUSE_CALL_ARG2, JAILHOUSE_CALL_CLOBBERED);
	return num_result;
}

static inline __jh_arg jailhouse_call_arg2(__jh_arg num, __jh_arg arg1,
					   __jh_arg arg2)
{
	register __jh_arg num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __jh_arg __arg1 asm(JAILHOUSE_CALL_ARG1) = arg1;
	register __jh_arg __arg2 asm(JAILHOUSE_CALL_ARG2) = arg2;

	asm volatile(
		JAILHOUSE_CALL_INS
		: "+r" (num_result), "+r" (__arg1), "+r" (__arg2)
		: : "memory", JAILHOUSE_CALL_CLOBBERED);
	return num_result;
}

static inline void
jailhouse_send_msg_to_cell(struct jailhouse_comm_region *comm_region,
			   __jh_arg msg)
{
	comm_region->reply_from_cell = JAILHOUSE_MSG_NONE;
	/* ensure reply was cleared before sending new message */
	asm volatile("dmb ishst" : : : "memory");
	comm_region->msg_to_cell = msg;
}

static inline void
jailhouse_send_reply_from_cell(struct jailhouse_comm_region *comm_region,
			       __jh_arg reply)
{
	comm_region->msg_to_cell = JAILHOUSE_MSG_NONE;
	/* ensure message was cleared before sending reply */
	asm volatile("dmb ishst" : : : "memory");
	comm_region->reply_from_cell = reply;
}

#endif /* !__ASSEMBLY__ */
