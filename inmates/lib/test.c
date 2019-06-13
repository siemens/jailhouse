/*
 *
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>
#include <test.h>

bool all_passed = true;

void __evaluate(u64 a, u64 b, int line)
{
	bool passed = (a == b);

	printk("Test at line #%d %s\n", line, passed ? "passed" : "FAILED");
	if (!passed) {
		printk(" %llx != %llx\n", (u64)a, (u64)b);
		all_passed = false;
	}
}
