/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 * Copyright (C) 2014 ARM Limited
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * Part of the fuctionality is derived from the AArch32 implementation, under
 * hypervisor/arch/arm/mmio.c by Jean-Philippe Brucker.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/entry.h>
#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <asm/bitops.h>
#include <jailhouse/percpu.h>
#include <asm/sysregs.h>
#include <asm/traps.h>

/* AARCH64_TODO: consider merging this with the AArch32 version */

static void arch_inject_dabt(struct trap_context *ctx, unsigned long addr)
{
	int err __attribute__((unused)) = trace_error(-EINVAL);
	while (1);
}

int arch_handle_dabt(struct trap_context *ctx)
{
	enum mmio_result mmio_result;
	struct mmio_access mmio;
	unsigned long hpfar;
	unsigned long hdfar;
	/* Decode the syndrome fields */
	u32 iss		= ESR_ISS(ctx->esr);
	u32 isv		= iss >> 24;
	u32 sas		= iss >> 22 & 0x3;
	u32 sse		= iss >> 21 & 0x1;
	u32 srt		= iss >> 16 & 0x1f;
	u32 ea		= iss >> 9 & 0x1;
	u32 cm		= iss >> 8 & 0x1;
	u32 s1ptw	= iss >> 7 & 0x1;
	u32 is_write	= iss >> 6 & 0x1;
	u32 size	= 1 << sas;

	arm_read_sysreg(HPFAR_EL2, hpfar);
	arm_read_sysreg(FAR_EL2, hdfar);
	mmio.address = hpfar << 8;
	mmio.address |= hdfar & 0xfff;

	this_cpu_public()->stats[JAILHOUSE_CPU_STAT_VMEXITS_MMIO]++;

	/*
	 * Invalid instruction syndrome means multiple access or writeback,
	 * there is nothing we can do.
	 */
	if (!isv)
		goto error_unhandled;

	/* Re-inject abort during page walk, cache maintenance or external */
	if (s1ptw || ea || cm) {
		arch_inject_dabt(ctx, hdfar);
		return TRAP_HANDLED;
	}

	if (is_write) {
		/* Load the value to write from the src register */
		mmio.value = (srt == 31) ? 0 : ctx->regs[srt];
		if (sse)
			mmio.value = sign_extend(mmio.value, 8 * size);
	} else {
		mmio.value = 0;
	}
	mmio.is_write = is_write;
	mmio.size = size;

	mmio_result = mmio_handle_access(&mmio);
	if (mmio_result == MMIO_ERROR)
		return TRAP_FORBIDDEN;
	if (mmio_result == MMIO_UNHANDLED)
		goto error_unhandled;

	/* Put the read value into the dest register */
	if (!is_write && (srt != 31)) {
		if (sse)
			mmio.value = sign_extend(mmio.value, 8 * size);
		ctx->regs[srt] = mmio.value;
	}

	arch_skip_instruction(ctx);
	return TRAP_HANDLED;

error_unhandled:
	panic_printk("Unhandled data %s at 0x%lx(%d)\n",
		(is_write ? "write" : "read"), mmio.address, size);

	return TRAP_UNHANDLED;
}
