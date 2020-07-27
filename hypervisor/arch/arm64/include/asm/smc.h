/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (C) 2018 OTH Regensburg
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

static inline long smc(unsigned long id)
{
	register unsigned long __id asm("r0") = id;

	asm volatile ("smc #0\n\t"
		: "+r" (__id)
		: : "memory", "x1", "x2", "x3");

	return __id;
}

static inline long smc_arg1(unsigned long id, unsigned long par1)
{
	register unsigned long __id asm("r0") = id;
	register unsigned long __par1 asm("r1") = par1;

	asm volatile ("smc #0\n\t"
		: "+r" (__id), "+r" (__par1)
		: : "memory", "x2", "x3");

	return __id;
}
