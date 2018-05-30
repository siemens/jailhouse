/*
 * Jailhouse ARM support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/sysregs.h>
#include <inmate.h>

void inmate_main(void)
{
	void register (*entry)(unsigned long, unsigned long, unsigned long);
	unsigned long register dtb, sctlr;

	entry = (void *)(unsigned long)cmdline_parse_int("kernel", 0);
	dtb = cmdline_parse_int("dtb", 0);

	/*
	 * Linux wants the MMU and D-caches to be disabled.
	 * As we didn't write anything relevant to the caches so far, we can
	 * get away without flushing.
	 */
	arm_read_sysreg(SCTLR, sctlr);
	sctlr &= ~(SCTLR_C | SCTLR_M);

	/*
	 * This is a pendant for
	 *   arm_write_sysreg(SCTLR, sctlr);
	 *   instruction_barrier();
	 *   entry(0, -1, dtb);
	 *
	 * After disabling the MMU, we must not touch the stack because we don't
	 * flush+inval dcaches, and the compiler might use the stack between
	 * calling entry. Assembly ensures that everything relevant is kept in
	 * registers.
	 */
	asm volatile(
		"mov r0, #0\n\t"
		"mov r1, #-1\n\t"
		"mov r2, %0\n\t"
		"mcr p15, 0, %1, c1, c0, 0\n\t"
		"isb\n\t"
		"bx %2\n\t" /* entry(0, -1, dtb) */
		: : "r" (dtb), "r" (sctlr), "r" (entry) : "r0", "r1", "r2");
}
