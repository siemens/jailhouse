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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inmate.h>

static inline int tolower(int c)
{
   return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

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

int memcmp(const void *s1, const void *s2, unsigned long n)
{
	const unsigned char *_s1 = s1, *_s2 = s2;

	while (n-- > 0)
		if (*_s1++ != *_s2++)
			return _s1[-1] < _s2[-1] ? -1 : 1;
	return 0;
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

int strncasecmp(const char *s1, const char *s2, unsigned long n)
{
	int diff;

	while (n-- > 0) {
		diff = tolower(*s1) - tolower(*s2);
		if (diff)
			return diff;
		if (*s1 == 0)
			break;
		s1++;
		s2++;
	}

	return 0;
}
