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

#define JAILHOUSE_BASE			0xf0000000

#define JAILHOUSE_CALL_INS		".arch_extension virt\n\t" \
					"hvc #0x4a48"
#define JAILHOUSE_CALL_NUM_RESULT	"r0"
#define JAILHOUSE_CALL_ARG		"r1"

#ifndef __asmeq
#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"
#endif

#ifndef __ASSEMBLY__

static inline __u32 jailhouse_call(__u32 num)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;

	asm volatile(
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%0")
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%1")
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result)
		: "memory");
	return num_result;
}

static inline __u32 jailhouse_call_arg(__u32 num, __u32 arg)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __u32 __arg asm(JAILHOUSE_CALL_ARG) = arg;

	asm volatile(
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%0")
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%1")
		__asmeq(JAILHOUSE_CALL_ARG, "%2")
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result), "r" (__arg)
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
