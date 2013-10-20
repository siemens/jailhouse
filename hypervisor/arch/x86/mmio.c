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
#include <asm/spinlock.h>
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

static DEFINE_SPINLOCK(mmio_lock);

struct mmio_access mmio_parse(struct per_cpu *cpu_data, unsigned long pc,
			      unsigned long page_table_addr, bool is_write)
{
	struct mmio_access access = { .inst_len = 0 };
	unsigned int cpu_id = cpu_data->cpu_id;
	struct cell *cell = cpu_data->cell;
	bool has_regr, has_modrm, does_write;
	struct modrm modrm;
	struct sib sib;
	u8 *page;

	spin_lock(&mmio_lock);

	access.inst_len = 0;
	has_regr = false;

restart:
	page = page_map_get_foreign_page(cpu_id, page_table_addr,
					 cell->page_offset, pc,
					 PAGE_DEFAULT_FLAGS);
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
		page = page_map_get_foreign_page(cpu_id, page_table_addr,
						 cell->page_offset, pc,
						 PAGE_DEFAULT_FLAGS);
		if (!page)
			goto error_nopage;

		modrm = *(struct modrm *)&page[pc & PAGE_OFFS_MASK];
		switch (modrm.mod) {
		case 0:
			if (modrm.rm != 4)
				goto error_unsupported;

			pc++;
			page = page_map_get_foreign_page(cpu_id,
							 page_table_addr,
							 cell->page_offset, pc,
							 PAGE_DEFAULT_FLAGS);
			if (!page)
				goto error_nopage;

			sib = *(struct sib *)&page[pc & PAGE_OFFS_MASK];
			if (sib.ss !=0 || sib.index != 4 || sib.reg != 5)
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

unmap_out:
	page_map_release_foreign_page(cpu_id);

	spin_unlock(&mmio_lock);
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
	goto unmap_out;
}
