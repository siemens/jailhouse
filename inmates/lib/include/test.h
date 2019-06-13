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

#define EXPECT_EQUAL(a, b)     __evaluate(a, b, __LINE__)

extern bool all_passed;

void __evaluate(u64 a, u64 b, int line);
