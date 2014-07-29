/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Ivan Kolchin <ivan.kolchin@siemens.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_PCI_H
#define _JAILHOUSE_PCI_H

#include <asm/cell.h>

#define PCI_BUS(bdf)		((bdf) >> 8)
#define PCI_DEVFN(bdf)		((bdf) & 0xff)
#define PCI_BDF_PARAMS(bdf)	(bdf) >> 8, ((bdf) >> 3) & 0x1f, (bdf) & 7

enum pci_access { PCI_ACCESS_REJECT, PCI_ACCESS_PERFORM, PCI_ACCESS_DONE };

struct pci_device {
	const struct jailhouse_pci_device *info;
	struct cell *cell;
};

int pci_init(void);

u32 pci_read_config(u16 bdf, u16 address, unsigned int size);
void pci_write_config(u16 bdf, u16 address, u32 value, unsigned int size);

struct pci_device *pci_get_assigned_device(const struct cell *cell, u16 bdf);

enum pci_access pci_cfg_read_moderate(struct pci_device *device, u16 address,
				      unsigned int size, u32 *value);
enum pci_access pci_cfg_write_moderate(struct pci_device *device, u16 address,
				       unsigned int size, u32 value);

int pci_mmio_access_handler(const struct cell *cell, bool is_write, u64 addr,
			    u32 *value);

int pci_cell_init(struct cell *cell);
void pci_cell_exit(struct cell *cell);

u32 arch_pci_read_config(u16 bdf, u16 address, unsigned int size);
void arch_pci_write_config(u16 bdf, u16 address, u32 value, unsigned int size);

#endif /* !_JAILHOUSE_PCI_H */
