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
#define PCI_CFG_BAR_END		0x27
#define PCI_CFG_ROMBAR		0x30
#define PCI_CFG_CAPS		0x34
#define PCI_CFG_INT		0x3c

#define PCI_CONFIG_HEADER_SIZE	0x40

#define PCI_NUM_BARS		6

#define PCI_DEV_CLASS_MEM	0x05

#define PCI_CAP_MSI		0x05
#define PCI_CAP_MSIX		0x11

#define PCI_IVSHMEM_NUM_MMIO_REGIONS	2

struct cell;

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

/** MSI-X vectors supported per device without extra allocation. */
#define PCI_EMBEDDED_MSIX_VECTS	16

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
	} __attribute__((packed));
	u32 raw;
	/** @publicsection */
} __attribute__((packed));

/** MSI-X table entry. See PCI specification. */
union pci_msix_vector {
	/** @privatesection */
	struct {
		u64 address;
		u32 data;
		u32 masked:1;
		u32 reserved:31;
	} __attribute__((packed));
	u32 raw[4];
	/** @publicsection */
} __attribute__((packed));

struct pci_ivshmem_endpoint;

/**
 * PCI device state.
 */
struct pci_device {
	/** Reference to static device configuration. */
	const struct jailhouse_pci_device *info;
	/** Owning cell. */
	struct cell *cell;
	/** Shadow BAR */
	u32 bar[PCI_NUM_BARS];

	/** Shadow state of MSI config space registers. */
	union pci_msi_registers msi_registers;

	/** Shadow state of MSI-X config space registers. */
	union pci_msix_registers msix_registers;
	/** Next PCI device in this cell with MSI-X support. */
	struct pci_device *next_msix_device;
	/** Next virtual PCI device in this cell. */
	struct pci_device *next_virtual_device;
	/** ivshmem specific data. */
	struct pci_ivshmem_endpoint *ivshmem_endpoint;
	/** Real MSI-X table. */
	union pci_msix_vector *msix_table;
	/** Shadow state of MSI-X table. */
	union pci_msix_vector *msix_vectors;
	/** Buffer for shadow table of up to PCI_EMBEDDED_MSIX_VECTS vectors. */
	union pci_msix_vector msix_vector_array[PCI_EMBEDDED_MSIX_VECTS];
};

unsigned int pci_mmio_count_regions(struct cell *cell);

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
 * Perform architecture-specific steps on physical PCI device addition.
 * @param cell		Cell to which the device is added.
 * @param device	Device to be added.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see arch_pci_remove_physical_device
 */
int arch_pci_add_physical_device(struct cell *cell, struct pci_device *device);

/**
 * Perform architecture-specific steps on physical PCI device removal.
 * @param device	Device to be removed.
 *
 * @see arch_pci_add_physical_device
 */
void arch_pci_remove_physical_device(struct pci_device *device);

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

/**
 * @defgroup PCI-IVSHMEM ivshmem
 * @{
 */
int pci_ivshmem_init(struct cell *cell, struct pci_device *device);
void pci_ivshmem_exit(struct pci_device *device);
int pci_ivshmem_update_msix(struct pci_device *device);
enum pci_access pci_ivshmem_cfg_write(struct pci_device *device,
				      unsigned int row, u32 mask, u32 value);
enum pci_access pci_ivshmem_cfg_read(struct pci_device *device, u16 address,
				     u32 *value);
/** @} PCI-IVSHMEM */
/** @} PCI */
#endif /* !_JAILHOUSE_PCI_H */
