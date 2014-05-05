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

union opcode {
	u8 raw;
	struct { /* REX */
		u8 b:1, x:1, r:1, w:1;
		u8 code:4;
	} __attribute__((packed)) rex;
	struct {
		u8 rm:3;
		u8 reg:3;
		u8 mod:2;
	} __attribute__((packed)) modrm;
	struct {
		u8 reg:3;
		u8 index:3;
		u8 ss:2;
	} __attribute__((packed)) sib;
};

/* If current_page is non-NULL, pc must have been increased exactly by 1. */
static u8 *map_code_page(struct per_cpu *cpu_data,
			 const struct guest_paging_structures *pg_structs,
			 unsigned long pc, u8 *current_page)
{
	/* If page offset is 0, previous pc was pointing to a different page,
	 * and we have to map a new one now. */
	if (current_page && ((pc & ~PAGE_MASK) != 0))
		return current_page;
	return page_map_get_guest_page(cpu_data, pg_structs, pc,
				       PAGE_READONLY_FLAGS);
}

struct mmio_access mmio_parse(struct per_cpu *cpu_data, unsigned long pc,
			      const struct guest_paging_structures *pg_structs,
			      bool is_write)
{
	struct mmio_access access = { .inst_len = 0 };
	bool has_rex_r = false;
	bool does_write;
	union opcode op[3];
	u8 *page = NULL;

restart:
	page = map_code_page(cpu_data, pg_structs, pc, page);
	if (!page)
		goto error_nopage;

	op[0].raw = page[pc & PAGE_OFFS_MASK];
	if (op[0].rex.code == X86_REX_CODE) {
		/* REX.W is simply over-read since it is only affects the
		 * memory address in our supported modes which we get from the
		 * virtualization support. */
		if (op[0].rex.r)
			has_rex_r = true;
		if (op[0].rex.x || op[0].rex.b)
			goto error_unsupported;

		pc++;
		access.inst_len++;
		goto restart;
	}
	switch (op[0].raw) {
	case X86_OP_MOV_TO_MEM:
		access.inst_len += 2;
		access.size = 4;
		does_write = true;
		break;
	case X86_OP_MOV_FROM_MEM:
		access.inst_len += 2;
		access.size = 4;
		does_write = false;
		break;
	default:
		goto error_unsupported;
	}

	pc++;
	page = map_code_page(cpu_data, pg_structs, pc, page);
	if (!page)
		goto error_nopage;

	op[1].raw = page[pc & PAGE_OFFS_MASK];
	switch (op[1].modrm.mod) {
	case 0:
		if (op[1].modrm.rm != 4)
			goto error_unsupported;

		pc++;
		page = map_code_page(cpu_data, pg_structs, pc, page);
		if (!page)
			goto error_nopage;

		op[2].raw = page[pc & PAGE_OFFS_MASK];
		if (op[2].sib.ss != 0 || op[2].sib.index != 4 ||
		    op[2].sib.reg != 5)
			goto error_unsupported;
		access.inst_len += 5;
		break;
	case 2:
		access.inst_len += 4;
		break;
	default:
		goto error_unsupported;
	}
	if (has_rex_r)
		access.reg = 7 - op[1].modrm.reg;
	else if (op[1].modrm.reg == 4)
		goto error_unsupported;
	else
		access.reg = 15 - op[1].modrm.reg;

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
