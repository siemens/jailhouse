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
 */

#include <inmate.h>
#include <test.h>
#include <asm/regs.h>

typedef u64 xmm_t __attribute__((vector_size(16)));

void inmate_main(void)
{
	xmm_t x_a, x_b, x_result;
	float f_addend, f_result;
	double d_a, d_b, d_result;

	printk("CPU supports\n"
	       "    FPU: %u   FXSR: %u XSAVE: %u\n"
	       "    SSE: %u   SSE2: %u  SSE3: %u\n"
	       " SSE4_1: %u SSE4_2: %u   AVX: %u\n"
	       " PCLMULQDQ: %u\n\n",
	       x86_cpu_features.fpu, x86_cpu_features.fxsr,
	       x86_cpu_features.xsave, x86_cpu_features.sse,
	       x86_cpu_features.sse2, x86_cpu_features.sse3,
	       x86_cpu_features.sse4_1, x86_cpu_features.sse4_2,
	       x86_cpu_features.avx, x86_cpu_features.pclmulqdq);

	if (x86_cpu_features.fpu) {
		f_addend = 123.45;
		f_result = 543.55;

		printk("Testing SSE...\n");
		asm volatile("addps %1, %0\t\n"
			     : "+x" (f_result) : "x" (f_addend));
		/* Test raw result */
		EXPECT_EQUAL(*(u32*)&f_result, 0x4426c000);
	}


	d_a = 123.45;
	d_b = 543.55;

	if (x86_cpu_features.sse2) {
		printk("Testing SSE2...\n");
		d_result = d_b;
		asm volatile("addsd %1, %0\t\n"
			     : "+x" (d_result) : "m" (d_a));
		EXPECT_EQUAL(d_result, 667);
	}

	if (x86_cpu_features.avx) {
		d_result = 0;
		printk("Testing AVX...\n");
		asm volatile("vaddsd %2, %1, %0\t\n"
			     : "=x" (d_result) : "x" (d_a), "m" (d_b));
		EXPECT_EQUAL(d_result, 667);
	}

	x_a[0] = 0x00017004200ab0cd;
	x_a[1] = 0xc000b802f6b31753;
	x_b[0] = 0xa0005c0252074a9a;
	x_b[1] = 0x50002e0207b1643c;

	if (x86_cpu_features.pclmulqdq && x86_cpu_features.avx) {
		printk("Testing AVX PCLMULQDQ...\n");
		asm volatile("vpclmulqdq %3, %2, %1, %0\t\n"
			     : "=x" (x_result) : "x" (x_a), "x" (x_b), "i" (0));

		EXPECT_EQUAL(x_result[0], 0x5ff61cc8b1043fa2);
		EXPECT_EQUAL(x_result[1], 0x00009602d147dc12);
	}

	if (x86_cpu_features.pclmulqdq) {
		printk("Testing PCLMULQDQ...\n");
		asm volatile("pclmulqdq %2, %1, %0\t\n"
			     : "+x" (x_a) : "x" (x_b), "i" (0));

		EXPECT_EQUAL(x_a[0], 0x5ff61cc8b1043fa2);
		EXPECT_EQUAL(x_a[1], 0x00009602d147dc12);
	}
}
