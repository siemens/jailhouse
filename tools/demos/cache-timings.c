/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2020
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <stdbool.h>
#include <stdio.h>

#define printk printf

typedef unsigned int u32;
typedef unsigned long long u64;

void inmate_main(void);

#include "../inmates/demos/x86/cache-timings-common.c"

int main(void)
{
	inmate_main();
	return 0;
}
