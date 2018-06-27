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

#ifndef _JAILHOUSE_CONSOLE_H
#define _JAILHOUSE_CONSOLE_H

/* Bits 0..3 are used to select the particular driver */
#define JAILHOUSE_CON1_TYPE_NONE	0x0000
#define JAILHOUSE_CON1_TYPE_VGA		0x0001
#define JAILHOUSE_CON1_TYPE_8250	0x0002
#define JAILHOUSE_CON1_TYPE_PL011	0x0003
#define JAILHOUSE_CON1_TYPE_XUARTPS	0x0004
#define JAILHOUSE_CON1_TYPE_MVEBU	0x0005
#define JAILHOUSE_CON1_TYPE_HSCIF	0x0006
#define JAILHOUSE_CON1_TYPE_SCIFA	0x0007
#define JAILHOUSE_CON1_TYPE_IMX		0x0008
#define JAILHOUSE_CON1_TYPE_MASK	0x000f

#define CON1_TYPE(flags) ((flags) & JAILHOUSE_CON1_TYPE_MASK)

/* Bits 4 is used to select PIO (cleared) or MMIO (set) access */
#define JAILHOUSE_CON1_ACCESS_PIO	0x0000
#define JAILHOUSE_CON1_ACCESS_MMIO	0x0010

#define CON1_IS_MMIO(flags) ((flags) & JAILHOUSE_CON1_ACCESS_MMIO)

/* Bits 5 is used to select 1 (cleared) or 4-bytes (set) register distance.
 * 1 byte implied 8-bit access, 4 bytes 32-bit access. */
#define JAILHOUSE_CON1_REGDIST_1	0x0000
#define JAILHOUSE_CON1_REGDIST_4	0x0020

#define CON1_USES_REGDIST_1(flags) (((flags) & JAILHOUSE_CON1_REGDIST_4) == 0)

/* Bits 8..11 are used to select the second console driver */
#define JAILHOUSE_CON2_TYPE_ROOTPAGE	0x0100
#define JAILHOUSE_CON2_TYPE_MASK	0x0f00

#define CON2_TYPE(flags) ((flags) & JAILHOUSE_CON2_TYPE_MASK)

struct jailhouse_console {
	__u64 address;
	__u32 size;
	__u32 flags;
	__u32 divider;
	__u32 gate_nr;
	__u64 clock_reg;
} __attribute__((packed));

#endif /* !_JAILHOUSE_CONSOLE_H */
