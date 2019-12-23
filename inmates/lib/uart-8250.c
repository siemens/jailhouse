/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
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
#include <uart.h>

#define UART_TX			0x0
#define UART_DLL		0x0
#define UART_DLM		0x1
#define UART_LCR		0x3
#define  UART_LCR_8N1		0x03
#define  UART_LCR_DLAB		0x80
#define UART_LSR		0x5
#define  UART_LSR_THRE		0x20
#define  UART_MDR1		0x8

static void reg_out_mmio32(struct uart_chip *chip, unsigned int reg, u32 value)
{
	        mmio_write32(chip->base + reg * 4, value);
}

static u32 reg_in_mmio32(struct uart_chip *chip, unsigned int reg)
{
	        return mmio_read32(chip->base + reg * 4);
}

static void uart_8250_init(struct uart_chip *chip)
{
	if (chip->divider) {
		chip->reg_out(chip, UART_LCR, UART_LCR_DLAB);
		chip->reg_out(chip, UART_DLL, chip->divider);
		chip->reg_out(chip, UART_DLM, 0);
		chip->reg_out(chip, UART_LCR, UART_LCR_8N1);
		if (CON_HAS_MDR_QUIRK(comm_region->console.flags))
			chip->reg_out(chip, UART_MDR1, 0);
	}
}

static bool uart_8250_is_busy(struct uart_chip *chip)
{
	return !(chip->reg_in(chip, UART_LSR) & UART_LSR_THRE);
}

static void uart_8250_write(struct uart_chip *chip, char c)
{
	chip->reg_out(chip, UART_TX, c);
}

DEFINE_UART_REG(8250, "8250", JAILHOUSE_CON_TYPE_8250,
		reg_out_mmio32, reg_in_mmio32);
