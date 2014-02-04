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

#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <asm/fault.h>

struct modrm {
	u8 rm:3;
	u8 reg:3;
	u8 mod:2;
} __attribute__((packed));

struct sib {
	u8 reg:3;
	u8 index:3;
	u8 ss:2;
} __attribute__((packed));

/* If current_page is non-NULL, pc must have been increased exactly by 1. */
static u8 *map_code_page(struct per_cpu *cpu_data, unsigned long pc,
			 unsigned long page_table_addr, u8 *current_page)
{
	/* If page offset is 0, previous pc was pointing to a different page,
	 * and we have to map a new one now. */
	if (current_page && ((pc & ~PAGE_MASK) != 0))
		return current_page;
	return page_map_get_guest_page(cpu_data, x86_64_paging,
				       page_table_addr, pc,
				       PAGE_READONLY_FLAGS);
}

struct mmio_access mmio_parse(struct per_cpu *cpu_data, unsigned long pc,
			      unsigned long page_table_addr, bool is_write)
{
	struct mmio_access access = { .inst_len = 0 };
	bool has_regr, has_modrm, does_write;
	struct modrm modrm;
	struct sib sib;
	u8 *page = NULL;

	access.inst_len = 0;
	has_regr = false;

restart:
	page = map_code_page(cpu_data, pc, page_table_addr, page);
	if (!page)
		goto error_nopage;

	has_modrm = false;
	switch (page[pc & PAGE_OFFS_MASK]) {
	case X86_OP_REGR_PREFIX:
		if (has_regr)
			goto error_unsupported;
		has_regr = true;
		pc++;
		access.inst_len++;
		goto restart;
	case X86_OP_MOV_TO_MEM:
		access.inst_len += 2;
		access.size = 4;
		has_modrm = true;
		does_write = true;
		break;
	case X86_OP_MOV_FROM_MEM:
		access.inst_len += 2;
		access.size = 4;
		has_modrm = true;
		does_write = false;
		break;
	default:
		goto error_unsupported;
	}

	if (has_modrm) {
		pc++;
		page = map_code_page(cpu_data, pc, page_table_addr, page);
		if (!page)
			goto error_nopage;

		modrm = *(struct modrm *)&page[pc & PAGE_OFFS_MASK];
		switch (modrm.mod) {
		case 0:
			if (modrm.rm != 4)
				goto error_unsupported;

			pc++;
			page = map_code_page(cpu_data, pc, page_table_addr,
					     page);
			if (!page)
				goto error_nopage;

			sib = *(struct sib *)&page[pc & PAGE_OFFS_MASK];
			if (sib.ss != 0 || sib.index != 4 || sib.reg != 5)
				goto error_unsupported;
			access.inst_len += 5;
			break;
		case 2:
			access.inst_len += 4;
			break;
		default:
			goto error_unsupported;
		}
		if (has_regr)
			access.reg = 7 - modrm.reg;
		else if (modrm.reg == 4)
			goto error_unsupported;
		else
			access.reg = 15 - modrm.reg;
	}

	if (does_write != is_write)
		goto error_inconsitent;

	return access;

error_nopage:
	panic_printk("FATAL: unable to map MMIO instruction page\n");
	goto error;

error_unsupported:
	panic_printk("FATAL: unsupported instruction\n");
	goto error;

error_inconsitent:
	panic_printk("FATAL: inconsistent access, expected %s instruction\n",
		     is_write ? "write" : "read");
error:
	access.inst_len = 0;
	return access;
}
