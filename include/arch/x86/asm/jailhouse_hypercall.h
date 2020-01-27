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
#define JAILHOUSE_CPU_STAT_VMEXITS_CPUID	JAILHOUSE_GENERIC_CPU_STATS + 3
#define JAILHOUSE_CPU_STAT_VMEXITS_XSETBV	JAILHOUSE_GENERIC_CPU_STATS + 4
#define JAILHOUSE_CPU_STAT_VMEXITS_EXCEPTION	JAILHOUSE_GENERIC_CPU_STATS + 5
#define JAILHOUSE_CPU_STAT_VMEXITS_MSR_OTHER	JAILHOUSE_GENERIC_CPU_STATS + 6
#define JAILHOUSE_CPU_STAT_VMEXITS_MSR_X2APIC_ICR \
						JAILHOUSE_GENERIC_CPU_STATS + 7
#define JAILHOUSE_NUM_CPU_STATS			JAILHOUSE_GENERIC_CPU_STATS + 8

/* CPUID interface */
#define JAILHOUSE_CPUID_SIGNATURE		0x40000000
#define JAILHOUSE_CPUID_FEATURES		0x40000001

/**
 * @defgroup Hypercalls Hypercall Subsystem
 *
 * The hypercall subsystem provides an interface for cells to invoke Jailhouse
 * services and interact via the communication region.
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

#ifdef DOXYGEN_CPP
/* included to expand COMM_REGION_GENERIC_HEADER */
#include <jailhouse/hypercall.h>
#endif

/** Communication region between hypervisor and a cell. */
struct jailhouse_comm_region {
	COMM_REGION_GENERIC_HEADER;

	/** I/O port address of the PM timer (x86-specific). */
	__u16 pm_timer_address;
	/** Number of CPUs available to the cell (x86-specific). */
	__u16 num_cpus;
	/** Calibrated TSC frequency in kHz (x86-specific). */
	__u32 tsc_khz;
	/** Calibrated APIC timer frequency in kHz or 0 if TSC deadline timer
	 * is available (x86-specific). */
	__u32 apic_khz;
} __attribute__((packed));

/**
 * Invoke a hypervisor without additional arguments.
 * @param num		Hypercall number.
 *
 * @return Result of the hypercall, semantic depends on the invoked service.
 */
static inline __u32 jailhouse_call(__u32 num)
{
	__u32 result;

	asm volatile(JAILHOUSE_CALL_CODE
		: JAILHOUSE_CALL_RESULT
		: JAILHOUSE_USE_VMCALL, JAILHOUSE_CALL_NUM
		: "memory");
	return result;
}

/**
 * Invoke a hypervisor with one argument.
 * @param num		Hypercall number.
 * @param arg1		First argument.
 *
 * @return Result of the hypercall, semantic depends on the invoked service.
 */
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

/**
 * Invoke a hypervisor with two arguments.
 * @param num		Hypercall number.
 * @param arg1		First argument.
 * @param arg2		Second argument.
 *
 * @return Result of the hypercall, semantic depends on the invoked service.
 */
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

/**
 * Send a message from the hypervisor to a cell via the communication region.
 * @param comm_region	Pointer to Communication Region.
 * @param msg		Message to be sent.
 */
static inline void
jailhouse_send_msg_to_cell(struct jailhouse_comm_region *comm_region,
			   __u32 msg)
{
	comm_region->reply_from_cell = JAILHOUSE_MSG_NONE;
	/* ensure reply was cleared before sending new message */
	asm volatile("mfence" : : : "memory");
	comm_region->msg_to_cell = msg;
}

/**
 * Send a reply message from a cell to the hypervisor via the communication
 * region.
 * @param comm_region	Pointer to Communication Region.
 * @param reply		Reply to be sent.
 */
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
