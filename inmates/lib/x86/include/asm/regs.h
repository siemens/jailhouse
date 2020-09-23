/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2019
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

#define X86_CR0_PE		0x00000001
#define X86_CR0_WP		0x00010000
#define X86_CR0_PG		0x80000000

#define X86_CR4_PAE		0x00000020
#define X86_CR4_PSE		0x00000010
#define X86_CR4_OSFXSR		0x00000200
#define X86_CR4_OSXSAVE		0x00040000

#define X86_XCR0_X87		(1 << 0)
#define X86_XCR0_SSE		(1 << 1)
#define X86_XCR0_AVX		(1 << 2)

#define PAGE_FLAG_PRESENT	0x01
#define PAGE_FLAG_RW		0x02
#define PAGE_FLAG_PS		0x80
#define PAGE_FLAG_PCD		0x10

#define PAGE_DEFAULT_FLAGS	(PAGE_FLAG_PRESENT | PAGE_FLAG_RW)

#define MSR_EFER		0xc0000080
#define EFER_LME		0x00000100

#define X86_CPUID_FEATURES	0x00000001 /* Processor Info and Feature Bits */
/* Feature bits in EDX */
# define X86_FEATURE_FPU	(1 << 0)  /* The processor contains an x87 FPU. */
# define X86_FEATURE_FXSR       (1 << 24) /* FXSAVE/FXRSTOR, CR4.OSFXSR */
# define X86_FEATURE_SSE	(1 << 25) /* The processor supports SSE */
# define X86_FEATURE_SSE2	(1 << 26) /* The processor supports SSE2 */
/* Feature bits in ECX */
# define X86_FEATURE_SSE3	(1 << 0)  /* The processor supports SSE3 */
# define X86_FEATURE_PCLMULQDQ	(1 << 1)  /* The processor supports PCLMULQDQ */
# define X86_FEATURE_SSE4_1	(1 << 19) /* The processor supports SSE4.1 */
# define X86_FEATURE_SSE4_2	(1 << 20) /* The processor supports SSE4.2 */
# define X86_FEATURE_XSAVE	(1 << 26) /* XSAVE/..., CR4.OSXSAVE */

#define X86_CPUID_XSTATE	0x0000000d /* Extended state features */

#define MSR_MTRR_DEF_TYPE	0x000002ff
#define MTRR_ENABLE		0x00000800

#ifndef __ASSEMBLY__

#include <string.h>

struct x86_cpu_features {
	bool avx:1;
	bool sse:1;
	bool sse2:1;
	bool sse3:1;
	bool sse4_1:1;
	bool sse4_2:1;
	bool fpu:1;
	bool xsave:1;
	bool fxsr:1;
	bool pclmulqdq:1;
};

extern struct x86_cpu_features x86_cpu_features;

#define READ_CR(n)					\
static inline unsigned long read_cr##n(void)		\
{							\
	unsigned long cr;				\
	asm volatile("mov %%cr" __stringify(n) ", %0"	\
		: "=r" (cr));				\
							\
	return cr;					\
}

READ_CR(3)
READ_CR(4)

static inline void write_cr4(unsigned long val)
{
	asm volatile("mov %0, %%cr4" : : "r" (val));
}

static inline u64 read_xcr0(void)
{
	register u32 eax, edx;

	asm volatile("xgetbv" : "=a" (eax), "=d" (edx) : "c" (0));

	return ((u64)(edx) << 32) + ((u64)(eax));
}

static inline void write_xcr0(u64 xcr0)
{
	unsigned int eax = xcr0;
	unsigned int edx = xcr0 >> 32;

	asm volatile("xsetbv" : : "a" (eax), "d" (edx), "c" (0));
}

static inline void cpuid(unsigned int *eax, unsigned int *ebx,
                         unsigned int *ecx, unsigned int *edx)
{
        /* ecx is often an input as well as an output. */
        asm volatile("cpuid"
            : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
            : "0" (*eax), "2" (*ecx)
            : "memory");
}

static inline u64 cpuid_edax(unsigned int op, unsigned int sub)
{
	unsigned int eax, ebx, ecx, edx;

	eax = op;
	ecx = sub;
	cpuid(&eax, &ebx, &ecx, &edx);
	return ((u64)edx << 32) + (u64)eax;
}

#define CPUID_REG(reg)							  \
static inline unsigned int cpuid_##reg(unsigned int op, unsigned int sub) \
{									  \
	unsigned int eax, ebx, ecx, edx;				  \
									  \
	eax = op;							  \
	ecx = sub;							  \
	cpuid(&eax, &ebx, &ecx, &edx);					  \
	return reg;							  \
}

CPUID_REG(eax)
CPUID_REG(ebx)
CPUID_REG(ecx)
CPUID_REG(edx)

#endif /* __ASSEMBLY__ */
