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

#include <inmate.h>
#include <asm/regs.h>

/* Must only be called from assembler header */
void arch_init_features(void);

struct x86_cpu_features x86_cpu_features;

/*
 * Every booting CPU will call this function before it enters its final C
 * entry. We make the assumption that all CPUs have the same feature set. So we
 * don't need any locks when writing to x86_cpu_features.
 */
void __attribute__((section(".boot"))) arch_init_features(void)
{
	u64 features;

	features = cpuid_edx(X86_CPUID_FEATURES, 0);
	/* Check availability of FPU */
	x86_cpu_features.fpu = !!(features & X86_FEATURE_FPU);

	/* Discover and enable FXSR */
	if (features & X86_FEATURE_FXSR) {
		write_cr4(read_cr4() | X86_CR4_OSFXSR);
		x86_cpu_features.fxsr = true;
	}

	/* Check availability of SSE */
	x86_cpu_features.sse = !!(features & X86_FEATURE_SSE);
	x86_cpu_features.sse2 = !!(features & X86_FEATURE_SSE2);

	/* ECX hides the rest */
	features = cpuid_ecx(X86_CPUID_FEATURES, 0);
	x86_cpu_features.sse3 = !!(features & X86_FEATURE_SSE3);
	x86_cpu_features.sse4_1 = !!(features & X86_FEATURE_SSE4_1);
	x86_cpu_features.sse4_2 = !!(features & X86_FEATURE_SSE4_2);
	x86_cpu_features.pclmulqdq = !!(features & X86_FEATURE_PCLMULQDQ);

	if (features & X86_FEATURE_XSAVE) {
		/* Enable XSAVE related instructions */
		write_cr4(read_cr4() | X86_CR4_OSXSAVE);
		x86_cpu_features.xsave = true;

		/*
		 * Intel SDM 13.2: A bit can be set in XCR0 if and only if the
		 * corresponding bit is set in this bitmap.  Every processor
		 * that supports the XSAVE feature set will set EAX[0] (x87
		 * state) and EAX[1] (SSE state).
		 *
		 * We can always set SSE + FP, but only set AVX if available.
		 */

		features = cpuid_edax(X86_CPUID_XSTATE, 0);
		write_xcr0(read_xcr0() | (features & X86_XCR0_AVX) | \
			   X86_XCR0_SSE | X86_XCR0_X87);
		x86_cpu_features.avx = !!(features & X86_XCR0_AVX);
	}

	/* hand control back to the header */
}
