/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
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
#include <uart.h>

static void reg_out_mmio8(struct uart_chip *chip, unsigned int reg, u32 value)
{
	mmio_write8(chip->base + reg, value);
}

static u32 reg_in_mmio8(struct uart_chip *chip, unsigned int reg)
{
	return mmio_read8(chip->base + reg);
}

void arch_console_init(struct uart_chip *chip)
{
	struct jailhouse_console *console = &comm_region->console;
	unsigned int gate_nr;
	bool gate_inverted;
	void *clock_reg;
	u32 clock_gates;

	gate_nr = cmdline_parse_int("con-gate-nr", console->gate_nr);
	gate_inverted = cmdline_parse_bool("con-gate-inverted",
					CON_HAS_INVERTED_GATE(console->flags));
	clock_reg = (void *)(unsigned long)
		cmdline_parse_int("con-clock-reg", console->clock_reg);

	if (cmdline_parse_bool("con-regdist-1",
			       CON_USES_REGDIST_1(console->flags))) {
		chip->reg_out = reg_out_mmio8;
		chip->reg_in = reg_in_mmio8;
	}

	if (chip->base)
		map_range(chip->base, PAGE_SIZE, MAP_UNCACHED);

	if (clock_reg) {
		map_range(clock_reg, PAGE_SIZE, MAP_UNCACHED);
		clock_gates = mmio_read32(clock_reg);
		if (gate_inverted)
			clock_gates &= ~(1 << gate_nr);
		else
			clock_gates |= (1 << gate_nr);
		mmio_write32(clock_reg, clock_gates);
	}
}
