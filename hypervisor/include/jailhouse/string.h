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

void *memcpy(void *d, const void *s, unsigned long n);
void *memset(void *s, int c, unsigned long n);

int strcmp(const char *s1, const char *s2);
