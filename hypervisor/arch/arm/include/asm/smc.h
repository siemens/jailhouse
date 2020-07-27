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

/* for gcc < 5 */
asm (".arch_extension sec\n");

static inline long smc(unsigned long id)
{
	register unsigned long __id asm("r0") = id;

	asm volatile ("smc #0\n\t"
		: "+r" (__id)
		: : "memory", "r1", "r2", "r3");

	return __id;
}

static inline int smc_arg1(unsigned long id, unsigned long par1)
{
	register unsigned long __id asm("r0") = id;
	register unsigned long __par1 asm("r1") = par1;

	asm volatile ("smc #0\n\t"
		: "+r" (__id), "+r" (__par1)
		: : "memory", "r2", "r3");

	return __id;
}

static inline long smc_arg2(unsigned long id, unsigned long par1,
			    unsigned long par2)
{
	register unsigned long __id asm("r0") = id;
	register unsigned long __par1 asm("r1") = par1;
	register unsigned long __par2 asm("r2") = par2;

	asm volatile ("smc #0\n\t"
		: "+r" (__id), "+r" (__par1), "+r" (__par2)
		: : "memory", "r3");

	return __id;
}
