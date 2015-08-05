/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <asm/ioapic.h>
#include <asm/iommu.h>
#include <asm/vcpu.h>

#define X86_MAX_INST_LEN	15

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
		u8 base:3;
		u8 index:3;
		u8 ss:2;
	} __attribute__((packed)) sib;
};

struct parse_context {
	unsigned int remaining;
	unsigned int size;
	const u8 *inst;
};

static void ctx_move_next_byte(struct parse_context *ctx)
{
	ctx->inst++;
	ctx->size--;
}

static bool ctx_maybe_get_bytes(struct parse_context *ctx,
				unsigned long *pc,
				const struct guest_paging_structures *pg)
{
	if (!ctx->size) {
		ctx->size = ctx->remaining;
		ctx->inst = vcpu_get_inst_bytes(pg, *pc, &ctx->size);
		if (!ctx->inst)
			return false;
		ctx->remaining -= ctx->size;
		*pc += ctx->size;
	}
	return true;
}

static bool ctx_advance(struct parse_context *ctx,
			unsigned long *pc,
			const struct guest_paging_structures *pg)
{
	ctx_move_next_byte(ctx);
	return ctx_maybe_get_bytes(ctx, pc, pg);
}

struct mmio_instruction x86_mmio_parse(unsigned long pc,
	const struct guest_paging_structures *pg_structs, bool is_write)
{
	struct parse_context ctx = { .remaining = X86_MAX_INST_LEN };
	struct mmio_instruction inst = { .inst_len = 0 };
	union opcode op[3] = { };
	bool has_rex_r = false;
	bool does_write;

restart:
	if (!ctx_maybe_get_bytes(&ctx, &pc, pg_structs))
		goto error_noinst;

	op[0].raw = *(ctx.inst);
	if (op[0].rex.code == X86_REX_CODE) {
		/* REX.W is simply over-read since it is only affects the
		 * memory address in our supported modes which we get from the
		 * virtualization support. */
		if (op[0].rex.r)
			has_rex_r = true;
		if (op[0].rex.x)
			goto error_unsupported;

		ctx_move_next_byte(&ctx);
		inst.inst_len++;
		goto restart;
	}
	switch (op[0].raw) {
	case X86_OP_MOV_TO_MEM:
		inst.inst_len += 2;
		inst.access_size = 4;
		does_write = true;
		break;
	case X86_OP_MOV_FROM_MEM:
		inst.inst_len += 2;
		inst.access_size = 4;
		does_write = false;
		break;
	default:
		goto error_unsupported;
	}

	if (!ctx_advance(&ctx, &pc, pg_structs))
		goto error_noinst;

	op[1].raw = *(ctx.inst);
	switch (op[1].modrm.mod) {
	case 0:
		if (op[1].modrm.rm == 5) /* 32-bit displacement */
			goto error_unsupported;
		else if (op[1].modrm.rm != 4) /* no SIB */
			break;
		inst.inst_len++;

		if (!ctx_advance(&ctx, &pc, pg_structs))
			goto error_noinst;

		op[2].raw = *(ctx.inst);
		if (op[2].sib.base == 5)
			inst.inst_len += 4;
		break;
	case 1:
	case 2:
		if (op[1].modrm.rm == 4) /* SIB */
			goto error_unsupported;
		inst.inst_len += op[1].modrm.mod == 1 ? 1 : 4;
		break;
	default:
		goto error_unsupported;
	}
	if (has_rex_r)
		inst.reg_num = 7 - op[1].modrm.reg;
	else if (op[1].modrm.reg == 4)
		goto error_unsupported;
	else
		inst.reg_num = 15 - op[1].modrm.reg;

	if (does_write != is_write)
		goto error_inconsitent;

	return inst;

error_noinst:
	panic_printk("FATAL: unable to get MMIO instruction\n");
	goto error;

error_unsupported:
	panic_printk("FATAL: unsupported instruction (0x%02x 0x%02x 0x%02x)\n",
		     op[0].raw, op[1].raw, op[2].raw);
	goto error;

error_inconsitent:
	panic_printk("FATAL: inconsistent access, expected %s instruction\n",
		     is_write ? "write" : "read");
error:
	inst.inst_len = 0;
	return inst;
}

unsigned int arch_mmio_count_regions(struct cell *cell)
{
	return ioapic_mmio_count_regions(cell) + iommu_mmio_count_regions(cell);
}
