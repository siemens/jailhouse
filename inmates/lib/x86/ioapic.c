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

#define IOAPIC_BASE		((void *)0xfec00000)
#define IOAPIC_REG_INDEX	0x00
#define IOAPIC_REG_DATA		0x10
#define IOAPIC_REDIR_TBL_START	0x10

void ioapic_init(void)
{
	map_range(IOAPIC_BASE, PAGE_SIZE, MAP_UNCACHED);
}

void ioapic_pin_set_vector(unsigned int pin,
			   enum ioapic_trigger_mode trigger_mode,
			   unsigned int vector)
{
	mmio_write32(IOAPIC_BASE + IOAPIC_REG_INDEX,
		     IOAPIC_REDIR_TBL_START + pin * 2 + 1);
	mmio_write32(IOAPIC_BASE + IOAPIC_REG_DATA, cpu_id() << (56 - 32));

	mmio_write32(IOAPIC_BASE + IOAPIC_REG_INDEX,
		     IOAPIC_REDIR_TBL_START + pin * 2);
	mmio_write32(IOAPIC_BASE + IOAPIC_REG_DATA, trigger_mode | vector);
}
