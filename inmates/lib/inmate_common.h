/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
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

#define NULL			((void *)0)

#define NS_PER_USEC		1000UL
#define NS_PER_MSEC		1000000UL
#define NS_PER_SEC		1000000000UL

#ifndef __ASSEMBLY__
typedef s8 __s8;
typedef u8 __u8;

typedef s16 __s16;
typedef u16 __u16;

typedef s32 __s32;
typedef u32 __u32;

typedef s64 __s64;
typedef u64 __u64;

typedef enum { true = 1, false = 0 } bool;

#include <jailhouse/hypercall.h>

#define comm_region	((struct jailhouse_comm_region *)COMM_REGION_BASE)

void printk(const char *fmt, ...);

void *memset(void *s, int c, unsigned long n);
void *memcpy(void *d, const void *s, unsigned long n);
unsigned long strlen(const char *s);
int strncmp(const char *s1, const char *s2, unsigned long n);
int strcmp(const char *s1, const char *s2);

const char *cmdline_parse_str(const char *param, char *value_buffer,
			      unsigned long buffer_size,
			      const char *default_value);
long long cmdline_parse_int(const char *param, long long default_value);
bool cmdline_parse_bool(const char *param);

#define CMDLINE_BUFFER(size) \
	const char cmdline[size] __attribute__((section(".cmdline")))

extern const char cmdline[];

void inmate_main(void);

#endif /* !__ASSEMBLY__ */
