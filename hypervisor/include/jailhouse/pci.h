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

#define PCI_CFG_COMMAND		0x04
# define PCI_CMD_MEM		(1 << 1)
# define PCI_CMD_MASTER		(1 << 2)
# define PCI_CMD_INTX_OFF	(1 << 10)
#define PCI_CFG_STATUS		0x06
# define PCI_STS_CAPS		(1 << 4)
#define PCI_CFG_BAR		0x10
# define PCI_BAR_64BIT		0x4
#define PCI_CFG_INT		0x3c

#define PCI_CONFIG_HEADER_SIZE	0x40

#define PCI_DEV_CLASS_MEM	0x05

#define PCI_CAP_MSI		0x05
#define PCI_CAP_MSIX		0x11

/**
 * @defgroup PCI PCI Subsystem
 *
 * The PCI subsystem provides access to PCI resources for the hypervisor and
 * manages the cell's access to device configuration spaces and MSI-X tables.
 * The subsystem depends on IOMMU services to configure DMA and interrupt
 * mappings.
 *
 * @{
 */

/** Extract PCI bus from BDF form. */
#define PCI_BUS(bdf)		((bdf) >> 8)
/** Extract PCI device/function from BDF form. */
#define PCI_DEVFN(bdf)		((bdf) & 0xff)
/** Extract PCI bus, device and function as parameter list from BDF form. */
#define PCI_BDF_PARAMS(bdf)	(bdf) >> 8, ((bdf) >> 3) & 0x1f, (bdf) & 7

/** Static limit of MSI-X vectors supported by Jailhouse per device. */
#define PCI_MAX_MSIX_VECTORS	16

/**
 * Access moderation return codes.
 * See pci_cfg_read_moderate() and pci_cfg_write_moderate().
 */
enum pci_access { PCI_ACCESS_REJECT, PCI_ACCESS_PERFORM, PCI_ACCESS_DONE };

/** MSI config space registers. See PCI specification. */
union pci_msi_registers {
	/** @privatesection */
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
	/** @publicsection */
} __attribute__((packed));

/** MSI-X config space registers. See PCI specification. */
union pci_msix_registers {
	/** @privatesection */
	struct {
		u16 padding;
		u16 ignore:14,
		    fmask:1,
		    enable:1;
	} __attribute__((packed)) field;
	u32 raw;
	/** @publicsection */
} __attribute__((packed));

/** MSI-X table entry. See PCI specification. */
union pci_msix_vector {
	/** @privatesection */
	struct {
		u64 address;
		u32 data;
		u32 ctrl;
	} __attribute__((packed)) field;
	u32 raw[4];
	/** @publicsection */
} __attribute__((packed));

/**
 * PCI device state.
 */
struct pci_device {
	/** Reference to static device configuration. */
	const struct jailhouse_pci_device *info;
	/** Owning cell. */
	struct cell *cell;

	/** Shadow state of MSI config space registers. */
	union pci_msi_registers msi_registers;

	/** Shadow state of MSI-X config space registers. */
	union pci_msix_registers msix_registers;
	/** Next PCI device in this cell with MSI-X support. */
	struct pci_device *next_msix_device;
	/** Next virtual PCI device in this cell. */
	struct pci_device *next_virtual_device;
	/** Real MSI-X table. */
	union pci_msix_vector *msix_table;
	/** Shadow state of MSI-X table. */
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

/**
 * Read from PCI config space via architecture-specific method.
 * @param bdf		16-bit bus/device/function ID of target.
 * @param address	Config space access address.
 * @param size		Access size (1, 2 or 4 bytes).
 *
 * @return Read value.
 *
 * @see pci_read_config
 * @see arch_pci_write_config
 */
u32 arch_pci_read_config(u16 bdf, u16 address, unsigned int size);

/**
 * Write to PCI config space via architecture-specific method.
 * @param bdf		16-bit bus/device/function ID of target.
 * @param address	Config space access address.
 * @param value		Value to be written.
 * @param size		Access size (1, 2 or 4 bytes).
 *
 * @see pci_write_config
 * @see arch_pci_read_config
 */
void arch_pci_write_config(u16 bdf, u16 address, u32 value, unsigned int size);

/**
 * Perform architecture-specific steps on PCI device addition.
 * @param cell		Cell to which the device is added.
 * @param device	Device to be added.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see arch_pci_remove_device
 */
int arch_pci_add_device(struct cell *cell, struct pci_device *device);

/**
 * Perform architecture-specific steps on PCI device removal.
 * @param device	Device to be removed.
 *
 * @see arch_pci_add_device
 */
void arch_pci_remove_device(struct pci_device *device);

/**
 * Avoid MSI vector delivery of a given device.
 * @param device	Device to be silenced.
 * @param cap		MSI capability of the device.
 */
void arch_pci_suppress_msi(struct pci_device *device,
			   const struct jailhouse_pci_capability *cap);

/**
 * Update MSI vector mapping for a given device.
 * @param device	Device to be updated.
 * @param cap		MSI capability of the device.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see arch_pci_update_msix_vector
 */
int arch_pci_update_msi(struct pci_device *device,
			const struct jailhouse_pci_capability *cap);

/**
 * Update MSI-X vector mapping for a given device and vector.
 * @param device	Device to be updated.
 * @param index		MSI-X vector number.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see arch_pci_update_msi
 */
int arch_pci_update_msix_vector(struct pci_device *device, unsigned int index);

/** @} */
#endif /* !_JAILHOUSE_PCI_H */
