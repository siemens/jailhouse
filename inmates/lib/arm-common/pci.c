/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2020
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

void pci_init(void)
{
	void *mmcfg = (void *)(unsigned long)comm_region->pci_mmconfig_base;

	if (mmcfg)
		map_range(mmcfg, 0x100000, MAP_UNCACHED);
}

static void *pci_get_device_mmcfg_base(u16 bdf)
{
	void *mmcfg = (void *)(unsigned long)comm_region->pci_mmconfig_base;

	return mmcfg + ((unsigned long)bdf << 12);
}

u32 pci_read_config(u16 bdf, unsigned int addr, unsigned int size)
{
	void *cfgaddr = pci_get_device_mmcfg_base(bdf) + addr;

	switch (size) {
	case 1:
		return mmio_read8(cfgaddr);
	case 2:
		return mmio_read16(cfgaddr);
	case 4:
		return mmio_read32(cfgaddr);
	default:
		return -1;
	}
}

void pci_write_config(u16 bdf, unsigned int addr, u32 value, unsigned int size)
{
	void *cfgaddr = pci_get_device_mmcfg_base(bdf) + addr;

	switch (size) {
	case 1:
		mmio_write8(cfgaddr, value);
		break;
	case 2:
		mmio_write16(cfgaddr, value);
		break;
	case 4:
		mmio_write32(cfgaddr, value);
		break;
	}
}

void pci_msix_set_vector(u16 bdf, unsigned int vector, u32 index)
{
	/* dummy for now, should never be called */
	*(int *)0xdeaddead = 0;
}
