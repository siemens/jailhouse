#ifndef _JAILHOUSE_STRING_H
#define _JAILHOUSE_STRING_H

/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */
#include <jailhouse/types.h>

void *memcpy(void *d, const void *s, size_t n);
void *memset(void *s, int c, size_t n);

int strcmp(const char *s1, const char *s2);

/*
 * Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.
 */

#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)

#endif
