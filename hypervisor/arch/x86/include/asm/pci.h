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

#ifndef _JAILHOUSE_ASM_PCI_H
#define _JAILHOUSE_ASM_PCI_H

#include <asm/percpu.h>
#include <asm/types.h>

/* --- PCI configuration ports --- */
#define PCI_REG_ADDR_PORT		0xcf8
#define PCI_REG_DATA_PORT		0xcfc

/* --- Address register fields --- */
/* Bits 31: Enable bit*/
#define PCI_ADDR_ENABLE			0x80000000
/* Bits 23-16: Bus number */
#define PCI_ADDR_BUS_MASK		0x00ff0000
/* Bits 15-11: Device number */
#define PCI_ADDR_DEV_MASK		0x0000f800
/* Bits 10-8: Function number */
#define PCI_ADDR_FUNC_MASK		0x00000700
/* Bits 7-2: Register number */
#define PCI_ADDR_REGNUM_MASK		0x000000fc
#define PCI_ADDR_VALID_MASK \
	(PCI_ADDR_ENABLE | PCI_ADDR_BUS_MASK | PCI_ADDR_DEV_MASK | \
	 PCI_ADDR_FUNC_MASK | PCI_ADDR_REGNUM_MASK)
#define PCI_ADDR_BDF_SHIFT		8

int x86_pci_config_handler(struct registers *guest_regs, struct cell *cell,
			   u16 port, bool dir_in, unsigned int size);

#endif /* !_JAILHOUSE_ASM_PCI_H */
