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

#define PCI_CFG_COMMAND		0x04
# define PCI_CMD_MASTER		(1 << 2)
# define PCI_CMD_INTX_OFF	(1 << 10)

#define PCI_MAX_MSIX_VECTORS	16

enum pci_access { PCI_ACCESS_REJECT, PCI_ACCESS_PERFORM, PCI_ACCESS_DONE };

union pci_msi_registers {
	struct {
		u16 padding;
		u16 enable:1,
		    ignore1:3,
		    mme:3,
		    ignore2:9;
		u32 address;
		u16 data;
	} __attribute__((packed)) msg32;
	struct {
		u32 padding; /* use msg32 */
		u64 address;
		u16 data;
	} __attribute__((packed)) msg64;
	u32 raw[4];
} __attribute__((packed));

union pci_msix_registers {
	struct {
		u16 padding;
		u16 ignore:14,
		    fmask:1,
		    enable:1;
	} __attribute__((packed)) field;
	u32 raw;
} __attribute__((packed));

union pci_msix_vector {
	struct {
		u64 address;
		u32 data;
		u32 ctrl;
	} __attribute__((packed)) field;
	u32 raw[4];
} __attribute__((packed));

struct pci_device {
	const struct jailhouse_pci_device *info;
	struct cell *cell;

	union pci_msi_registers msi_registers;

	union pci_msix_registers msix_registers;
	struct pci_device *next_msix_device;
	union pci_msix_vector *msix_table;
	union pci_msix_vector msix_vectors[PCI_MAX_MSIX_VECTORS];
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

void pci_config_commit(struct cell *cell_added_removed);

unsigned int pci_enabled_msi_vectors(struct pci_device *device);

void pci_prepare_handover(void);
void pci_shutdown(void);

u32 arch_pci_read_config(u16 bdf, u16 address, unsigned int size);
void arch_pci_write_config(u16 bdf, u16 address, u32 value, unsigned int size);

int arch_pci_add_device(struct cell *cell, struct pci_device *device);
void arch_pci_remove_device(struct pci_device *device);

void arch_pci_suppress_msi(struct pci_device *device,
			   const struct jailhouse_pci_capability *cap);
int arch_pci_update_msi(struct pci_device *device,
			const struct jailhouse_pci_capability *cap);
int arch_pci_update_msix_vector(struct pci_device *device, unsigned int index);

#endif /* !_JAILHOUSE_PCI_H */
