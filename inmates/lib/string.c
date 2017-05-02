/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

void *memcpy(void *dest, const void *src, unsigned long n)
{
	const u8 *s = src;
	u8 *d = dest;

	while (n-- > 0)
		*d++ = *s++;
	return dest;
}

void *memset(void *s, int c, unsigned long n)
{
	u8 *p = s;

	while (n-- > 0)
		*p++ = c;
	return s;
}

unsigned long strlen(const char *s1)
{
	unsigned long len = 0;

	while (*s1++)
		len++;

	return len;
}

int strncmp(const char *s1, const char *s2, unsigned long n)
{
	int diff;

	while (n-- > 0) {
		diff = *s1 - *s2;
		if (diff)
			return diff;
		if (*s1 == 0)
			break;
		s1++;
		s2++;
	}
	return 0;
}

int strcmp(const char *s1, const char *s2)
{
	return strncmp(s1, s2, -1);
}
