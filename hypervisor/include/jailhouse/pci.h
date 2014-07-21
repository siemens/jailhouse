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

enum pci_access { PCI_ACCESS_REJECT, PCI_ACCESS_PERFORM, PCI_ACCESS_EMULATE };

int pci_init(void);

const struct jailhouse_pci_device *
pci_get_assigned_device(const struct cell *cell, u16 bdf);

enum pci_access
pci_cfg_read_moderate(const struct cell *cell,
		      const struct jailhouse_pci_device *device, u8 reg_num,
		      unsigned int reg_bias, unsigned int size, u32 *value);
enum pci_access
pci_cfg_write_moderate(const struct cell *cell,
		       const struct jailhouse_pci_device *device, u8 reg_num,
		       unsigned int reg_bias, unsigned int size, u32 *value);

int pci_mmio_access_handler(const struct cell *cell, bool is_write, u64 addr,
			    u32 *value);

#endif /* !_JAILHOUSE_PCI_H */
