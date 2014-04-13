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

#ifndef __x86_64__
#error 64-bit kernel required!
#endif

#define JAILHOUSE_BASE		0xfffffffff0000000

#define JAILHOUSE_CALL_INS	"vmcall"
#define JAILHOUSE_CALL_RESULT	"=a" (result)
#define JAILHOUSE_CALL_NUM	"a" (num)
#define JAILHOUSE_CALL_ARG	"D" (arg)

#ifndef __ASSEMBLY__

static inline __u32 jailhouse_call(__u32 num)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_INS
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_CALL_NUM
		: "memory");
	return result;
}

static inline __u32 jailhouse_call_arg(__u32 num, __u32 arg)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_INS
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_CALL_NUM, JAILHOUSE_CALL_ARG
		: "memory");
	return result;
}

static inline void
jailhouse_send_msg_to_cell(struct jailhouse_comm_region *comm_region,
			   __u32 msg)
{
	comm_region->reply_from_cell = JAILHOUSE_MSG_NONE;
	/* ensure reply was cleared before sending new message */
	asm volatile("mfence" : : : "memory");
	comm_region->msg_to_cell = msg;
}

static inline void
jailhouse_send_reply_from_cell(struct jailhouse_comm_region *comm_region,
			       __u32 reply)
{
	comm_region->msg_to_cell = JAILHOUSE_MSG_NONE;
	/* ensure message was cleared before sending reply */
	asm volatile("mfence" : : : "memory");
	comm_region->reply_from_cell = reply;
}

#endif /* !__ASSEMBLY__ */
