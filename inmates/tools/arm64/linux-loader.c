/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/sysregs.h>
#include <inmate.h>

void inmate_main(void)
{
	unsigned long dtb, sctlr;
	void (*entry)(u64 dtb, u64 x1, u64 x2, u64 x3);

	entry = (void *)cmdline_parse_int("kernel", 0);
	dtb = cmdline_parse_int("dtb", 0);

	/*
	 * Linux wants the MMU to be disabled
	 * As we didn't write anything relevant to the caches so far, we can
	 * get away without flushing.
	 */
	arm_read_sysreg(SCTLR_EL1, sctlr);
	sctlr &= ~SCTLR_EL1_M;

	/*
	 * This is a pendant for
	 *   arm_write_sysreg(SCTLR_EL1, sctlr);
	 *   instruction_barrier();
	 *   entry(dtb, 0, 0, 0);
	 *
	 * After disabling the MMU, we must not touch the stack because we don't
	 * flush+inval dcaches, and the compiler might use the stack between
	 * calling entry. Assembly ensures that everything relevant is kept in
	 * registers.
	 */
	asm volatile(
		"mov x0, %0\n\t"
		"mov x1, #0\n\t"
		"mov x2, #0\n\t"
		"mov x3, #0\n\t"
		"msr sctlr_el1, %1\n\t"
		"isb\n\t"
		"br %2" /* entry(dtb, 0, 0, 0) */
		: : "r" (dtb), "r" (sctlr), "r" (entry)
		: "x0", "x1", "x2", "x3");
}
