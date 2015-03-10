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

#define JAILHOUSE_BASE		__MAKE_UL(0xfffffffff0000000)

/*
 * As this is never called on a CPU without VM extensions,
 * we assume that where VMCALL isn't available, VMMCALL is.
 */
#define JAILHOUSE_CALL_CODE	\
	"cmpb $0x01, %[use_vmcall]\n\t"\
	"jne 1f\n\t"\
	"vmcall\n\t"\
	"jmp 2f\n\t"\
	"1: vmmcall\n\t"\
	"2:"

#define JAILHOUSE_CALL_RESULT	"=a" (result)
#define JAILHOUSE_USE_VMCALL	[use_vmcall] "m" (jailhouse_use_vmcall)
#define JAILHOUSE_CALL_NUM	"a" (num)
#define JAILHOUSE_CALL_ARG1	"D" (arg1)
#define JAILHOUSE_CALL_ARG2	"S" (arg2)

/* CPU statistics */
#define JAILHOUSE_CPU_STAT_VMEXITS_PIO		JAILHOUSE_GENERIC_CPU_STATS
#define JAILHOUSE_CPU_STAT_VMEXITS_XAPIC	JAILHOUSE_GENERIC_CPU_STATS + 1
#define JAILHOUSE_CPU_STAT_VMEXITS_CR		JAILHOUSE_GENERIC_CPU_STATS + 2
#define JAILHOUSE_CPU_STAT_VMEXITS_MSR		JAILHOUSE_GENERIC_CPU_STATS + 3
#define JAILHOUSE_CPU_STAT_VMEXITS_CPUID	JAILHOUSE_GENERIC_CPU_STATS + 4
#define JAILHOUSE_CPU_STAT_VMEXITS_XSETBV	JAILHOUSE_GENERIC_CPU_STATS + 5
#define JAILHOUSE_NUM_CPU_STATS			JAILHOUSE_GENERIC_CPU_STATS + 6

#ifdef __ASSEMBLY__

#define __MAKE_UL(x)	x

#else /* !__ASSEMBLY__ */

#define __MAKE_UL(x)	x ## UL

/**
 * @defgroup Hypercalls
 *
 * The hypercall subsystem provides an interface for cells
 * to call into Jailhouse.
 *
 * @{
 */

/**
 * This variable selects the x86 hypercall instruction to be used by
 * jailhouse_call(), jailhouse_call_arg1(), and jailhouse_call_arg2().
 * A caller should define and initialize the variable before calling
 * any of these functions.
 *
 * @li @c false Use AMD's VMMCALL.
 * @li @c true Use Intel's VMCALL.
 */
extern bool jailhouse_use_vmcall;

struct jailhouse_comm_region {
	COMM_REGION_GENERIC_HEADER;

	__u16 pm_timer_address;
};

static inline __u32 jailhouse_call(__u32 num)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_CODE
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_USE_VMCALL, JAILHOUSE_CALL_NUM
		: "memory");
	return result;
}

static inline __u32 jailhouse_call_arg1(__u32 num, unsigned long arg1)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_CODE
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_USE_VMCALL,
		  JAILHOUSE_CALL_NUM, JAILHOUSE_CALL_ARG1
		: "memory");
	return result;
}

static inline __u32 jailhouse_call_arg2(__u32 num, unsigned long arg1,
					unsigned long arg2)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_CODE
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_USE_VMCALL,
		  JAILHOUSE_CALL_NUM, JAILHOUSE_CALL_ARG1, JAILHOUSE_CALL_ARG2
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

/** @} **/

#endif /* !__ASSEMBLY__ */
