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
 */

#include <inmate.h>

#include "../pci.c"

#define PCI_REG_ADDR_PORT	0xcf8
#define PCI_REG_DATA_PORT	0xcfc

#define PCI_CONE		(1 << 31)

u32 pci_read_config(u16 bdf, unsigned int addr, unsigned int size)
{
	outl(PCI_CONE | ((u32)bdf << 8) | (addr & 0xfc), PCI_REG_ADDR_PORT);
	switch (size) {
	case 1:
		return inb(PCI_REG_DATA_PORT + (addr & 0x3));
	case 2:
		return inw(PCI_REG_DATA_PORT + (addr & 0x3));
	case 4:
		return inl(PCI_REG_DATA_PORT);
	default:
		return -1;
	}
}

void pci_write_config(u16 bdf, unsigned int addr, u32 value, unsigned int size)
{
	outl(PCI_CONE | ((u32)bdf << 8) | (addr & 0xfc), PCI_REG_ADDR_PORT);
	switch (size) {
	case 1:
		outb(value, PCI_REG_DATA_PORT + (addr & 0x3));
		break;
	case 2:
		outw(value, PCI_REG_DATA_PORT + (addr & 0x3));
		break;
	case 4:
		outl(value, PCI_REG_DATA_PORT);
		break;
	}
}

void pci_msi_set_vector(u16 bdf, unsigned int vector)
{
	int cap = pci_find_cap(bdf, PCI_CAP_MSI);
	u16 ctl, data;

	if (cap < 0)
		return;

	pci_write_config(bdf, cap + 0x04, 0xfee00000 | (cpu_id() << 12), 4);

	ctl = pci_read_config(bdf, cap + 0x02, 2);
	if (ctl & (1 << 7)) {
		pci_write_config(bdf, cap + 0x08, 0, 4);
		data = cap + 0x0c;
	} else
		data = cap + 0x08;
	pci_write_config(bdf, data, vector, 2);

	pci_write_config(bdf, cap + 0x02, 0x0001, 2);
}
