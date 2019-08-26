/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2018
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
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
#include <asm/sysregs.h>

static u64 __attribute__((aligned(4096)))
	page_directory[JAILHOUSE_INMATE_MEM_PAGE_DIR_LEN];

void map_range(void *start, unsigned long size, enum map_type map_type)
{
	u64 vaddr, pmd_entry;
	unsigned pgd_index;
	u64 *pmd;

	vaddr = (unsigned long)start;

	size += (vaddr & ~HUGE_PAGE_MASK) + HUGE_PAGE_SIZE - 1;
	size &= HUGE_PAGE_MASK;

	while (size) {
		pgd_index = PGD_INDEX(vaddr);
		if (!(page_directory[pgd_index] & LONG_DESC_TABLE)) {
			pmd = alloc(PAGE_SIZE, PAGE_SIZE);
			memset(pmd, 0, PAGE_SIZE);
			/* ensure the page table walker will see the zeroes */
			synchronization_barrier();

			page_directory[pgd_index] =
				(unsigned long)pmd | LONG_DESC_TABLE;
		} else {
			pmd = (u64*)(unsigned long)
				(page_directory[pgd_index] & ~LONG_DESC_TABLE);
		}

		pmd_entry = vaddr & HUGE_PAGE_MASK;
		pmd_entry |= LATTR_AF | LATTR_INNER_SHAREABLE | \
			     LATTR_AP_RW_EL1 | LONG_DESC_BLOCK;
		if (map_type == MAP_CACHED)
			pmd_entry |= LATTR_MAIR(0);
		else
			pmd_entry |= LATTR_MAIR(1);

		pmd[PMD_INDEX(vaddr)] = pmd_entry;

		size -= HUGE_PAGE_SIZE;
		vaddr += HUGE_PAGE_SIZE;
	}

	/*
	 * As long es we only add entries and do not modify entries, a
	 * synchronization barrier is enough to propagate changes. Otherwise we
	 * need to flush the TLB.
	 */
	synchronization_barrier();
}

void arch_mmu_enable(void)
{
	unsigned long mair, sctlr;

	map_range((void*)CONFIG_INMATE_BASE, 0x10000, MAP_CACHED);
	map_range((void*)COMM_REGION_BASE, PAGE_SIZE, MAP_CACHED);

	/*
	 * ARMv7: Use attributes 0 and 1 in MAIR0
	 * ARMv8: Use attributes 0 and 1 in MAIR
	 *
	 * Attributes 0: inner/outer: normal memory, outer write-back
	 *		 non-transient
	 * Attributes 1: device memory
	 */
	mair = MAIR_ATTR(1, MAIR_ATTR_DEVICE) | MAIR_ATTR(0, MAIR_ATTR_WBRWA);
	arm_write_sysreg(MAIR, mair);

	arm_write_sysreg(TRANSL_CONT_REG, TRANSL_CONT_REG_SETTINGS);

	arm_write_sysreg(TTBR0, page_directory);
	/* This barrier ensures that TTBR0 is set before enabling the MMU. */
	instruction_barrier();

	arm_read_sysreg(SCTLR, sctlr);
	sctlr |= SCTLR_MMU_CACHES;
	arm_write_sysreg(SCTLR, sctlr);
	/* This barrier ensures that the MMU is actually on */
	instruction_barrier();
	/* MMU is enabled from now on */
}
