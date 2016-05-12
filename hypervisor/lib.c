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

#include <jailhouse/string.h>
#include <jailhouse/types.h>

void *memset(void *s, int c, unsigned long n)
{
	u8 *p = s;

	while (n-- > 0)
		*p++ = c;
	return s;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2) {
		if (*s1 == '\0')
			return 0;
		s1++;
		s2++;
	}
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void *memcpy(void *dest, const void *src, unsigned long n)
{
	const u8 *s = src;
	u8 *d = dest;

	while (n-- > 0)
		*d++ = *s++;
	return dest;
}
