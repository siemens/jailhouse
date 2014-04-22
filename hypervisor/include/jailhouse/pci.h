/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Ivan Kolchin <ivan.kolchin@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_PCI_H
#define _JAILHOUSE_PCI_H

#include <asm/cell.h>

#define PCI_CONFIG_HEADER_SIZE		0x40

int pci_init(void);

const struct jailhouse_pci_device *
pci_get_assigned_device(const struct cell *cell, u16 bdf);

bool pci_cfg_write_allowed(u32 type, u8 reg_num, unsigned int reg_bias,
			   unsigned int size);

int pci_mmio_access_handler(struct registers *guest_regs,
			    const struct cell *cell, bool is_write,
			    u64 addr, u32 reg);

#endif /* !_JAILHOUSE_PCI_H */
