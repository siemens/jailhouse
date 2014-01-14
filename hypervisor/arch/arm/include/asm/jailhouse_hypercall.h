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

#define JAILHOUSE_CALL_INS		".arch_extension virt\n\t" \
					"hvc #0x4a48"
#define JAILHOUSE_CALL_NUM_RESULT	"r0"
#define JAILHOUSE_CALL_ARG1		"r1"
#define JAILHOUSE_CALL_ARG2		"r2"
#define JAILHOUSE_CALL_ARG3		"r3"
#define JAILHOUSE_CALL_ARG4		"r4"

static inline __u32 jailhouse_call0(__u32 num)
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

static inline __u32 jailhouse_call1(__u32 num, __u32 arg1)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __u32 __arg1 asm(JAILHOUSE_CALL_ARG1) = arg1;

	asm volatile(
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%0")
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%1")
		__asmeq(JAILHOUSE_CALL_ARG1, "%2")
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result), "r" (__arg1)
		: "memory");
	return num_result;
}

static inline __u32 jailhouse_call2(__u32 num, __u32 arg1, __u32 arg2)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __u32 __arg1 asm(JAILHOUSE_CALL_ARG1) = arg1;
	register __u32 __arg2 asm(JAILHOUSE_CALL_ARG2) = arg2;

	asm volatile(
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%0")
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%1")
		__asmeq(JAILHOUSE_CALL_ARG1, "%2")
		__asmeq(JAILHOUSE_CALL_ARG2, "%3")
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result), "r" (__arg1), "r" (__arg2)
		: "memory");
	return num_result;
}

static inline __u32 jailhouse_call3(__u32 num, __u32 arg1, __u32 arg2,
				   __u32 arg3)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __u32 __arg1 asm(JAILHOUSE_CALL_ARG1) = arg1;
	register __u32 __arg2 asm(JAILHOUSE_CALL_ARG2) = arg2;
	register __u32 __arg3 asm(JAILHOUSE_CALL_ARG3) = arg3;

	asm volatile(
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%0")
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%1")
		__asmeq(JAILHOUSE_CALL_ARG1, "%2")
		__asmeq(JAILHOUSE_CALL_ARG2, "%3")
		__asmeq(JAILHOUSE_CALL_ARG3, "%4")
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result), "r" (__arg1), "r" (__arg2), "r" (__arg3)
		: "memory");
	return num_result;
}

static inline __u32 jailhouse_call4(__u32 num, __u32 arg1, __u32 arg2,
				   __u32 arg3, __u32 arg4)
{
	register __u32 num_result asm(JAILHOUSE_CALL_NUM_RESULT) = num;
	register __u32 __arg1 asm(JAILHOUSE_CALL_ARG1) = arg1;
	register __u32 __arg2 asm(JAILHOUSE_CALL_ARG2) = arg2;
	register __u32 __arg3 asm(JAILHOUSE_CALL_ARG3) = arg3;
	register __u32 __arg4 asm(JAILHOUSE_CALL_ARG4) = arg4;

	asm volatile(
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%0")
		__asmeq(JAILHOUSE_CALL_NUM_RESULT, "%1")
		__asmeq(JAILHOUSE_CALL_ARG1, "%2")
		__asmeq(JAILHOUSE_CALL_ARG2, "%3")
		__asmeq(JAILHOUSE_CALL_ARG3, "%4")
		__asmeq(JAILHOUSE_CALL_ARG4, "%5")
		JAILHOUSE_CALL_INS
		: "=r" (num_result)
		: "r" (num_result), "r" (__arg1), "r" (__arg2), "r" (__arg3),
		  "r" (__arg4)
		: "memory");
	return num_result;
}
