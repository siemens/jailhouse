/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
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
#include <asm/regs.h>

void map_range(void *start, unsigned long size, enum map_type map_type)
{
	unsigned long pt_addr, *pt_entry, *pt;
	unsigned long vaddr = (unsigned long)start;

	pt_addr = read_cr3();

	size += (vaddr & ~HUGE_PAGE_MASK) + HUGE_PAGE_SIZE - 1;
	size &= HUGE_PAGE_MASK;
	while (size > 0) {
#ifdef __x86_64__
		pt_addr &= PAGE_MASK;
		pt = (unsigned long *)pt_addr;

		pt_entry = &pt[(vaddr >> 39) & 0x1ff];
		if (*pt_entry & PAGE_FLAG_PRESENT) {
			pt = (unsigned long *)(*pt_entry & PAGE_MASK);
		} else {
			pt = zalloc(PAGE_SIZE, PAGE_SIZE);
			*pt_entry = (unsigned long)pt | PAGE_DEFAULT_FLAGS;
		}

		pt_entry = &pt[(vaddr >> 30) & 0x1ff];
		if (*pt_entry & PAGE_FLAG_PRESENT) {
			pt = (unsigned long *)(*pt_entry & PAGE_MASK);
		} else {
			pt = zalloc(PAGE_SIZE, PAGE_SIZE);
			*pt_entry = (unsigned long)pt | PAGE_DEFAULT_FLAGS;
		}

		pt_entry = &pt[(vaddr >> 21) & 0x1ff];
		*pt_entry = (vaddr & HUGE_PAGE_MASK) |
			(map_type == MAP_UNCACHED ? PAGE_FLAG_PCD : 0) |
			PAGE_FLAG_PS | PAGE_DEFAULT_FLAGS;
#else
#error not yet implemented
#endif
		size -= HUGE_PAGE_SIZE;
		vaddr += HUGE_PAGE_SIZE;
	}
}
