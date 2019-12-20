/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Henning Schild <henning.schild@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */
#include <jailhouse/entry.h>
#include <jailhouse/gcov.h>

extern unsigned long __init_array_start[], __init_array_end[];

/* the actual data structure is bigger but we just need to know the version
 * independent beginning to link the elements to a list */
struct gcov_min_info {
	unsigned int		version;
	struct gcov_min_info	*next;
};

void gcov_init(void) {
	unsigned long *iarray = __init_array_start;
	unsigned long *iarray_end = __init_array_end;
	void (*__init_func)(void);

	while (iarray < iarray_end) {
		__init_func = (void(*)(void))iarray[0];
		iarray++;
		__init_func();
	}
}

void __gcov_init(struct gcov_min_info *info);
void __gcov_merge_add(void *counters, unsigned int n_counters);
void __gcov_exit(void);

/* just link them all together and leave the head in the header
 * where a processing tool can find it */
void __gcov_init(struct gcov_min_info *info)
{
	info->next = (struct gcov_min_info *)hypervisor_header.gcov_info_head;
	hypervisor_header.gcov_info_head = info;
}

/* Satisfy the linker, never called */
void __gcov_merge_add(void *counters, unsigned int n_counters)
{
}

void __gcov_exit(void)
{
}
