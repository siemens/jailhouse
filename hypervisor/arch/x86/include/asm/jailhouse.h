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

#define JAILHOUSE_CALL_INS	"vmcall"
#define JAILHOUSE_CALL_RESULT	"=a" (result)
#define JAILHOUSE_CALL_NUM	"a" (num)
#define JAILHOUSE_CALL_ARG1	"D" (arg1)
#define JAILHOUSE_CALL_ARG2	"S" (arg2)
#define JAILHOUSE_CALL_ARG3	"d" (arg3)
#define JAILHOUSE_CALL_ARG4	"c" (arg4)

static inline __u32 jailhouse_call0(__u32 num)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_INS
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_CALL_NUM
		: "memory");
	return result;
}

static inline __u32 jailhouse_call1(__u32 num, __u32 arg1)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_INS
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_CALL_NUM, JAILHOUSE_CALL_ARG1
		: "memory");
	return result;
}

static inline __u32 jailhouse_call2(__u32 num, __u32 arg1, __u32 arg2)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_INS
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_CALL_NUM, JAILHOUSE_CALL_ARG1, JAILHOUSE_CALL_ARG2
		: "memory");
	return result;
}

static inline __u32 jailhouse_call3(__u32 num, __u32 arg1, __u32 arg2,
				   __u32 arg3)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_INS
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_CALL_NUM, JAILHOUSE_CALL_ARG1, JAILHOUSE_CALL_ARG2,
		  JAILHOUSE_CALL_ARG3
		: "memory");
	return result;
}

static inline __u32 jailhouse_call4(__u32 num, __u32 arg1, __u32 arg2,
				   __u32 arg3, __u32 arg4)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_INS
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_CALL_NUM, JAILHOUSE_CALL_ARG1, JAILHOUSE_CALL_ARG2,
		  JAILHOUSE_CALL_ARG3, JAILHOUSE_CALL_ARG4
		: "memory");
	return result;
}
