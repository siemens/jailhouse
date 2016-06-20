/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2016
 *
 * Authors:
 *  Henning Schild <henning.schild@siemens.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_IVSHMEM_H
#define _JAILHOUSE_IVSHMEM_H

#include <jailhouse/pci.h>
#include <asm/ivshmem.h>

#define IVSHMEM_CFG_MSIX_CAP	0x50
#define IVSHMEM_CFG_SIZE	(IVSHMEM_CFG_MSIX_CAP + 12)

/**
 * @defgroup IVSHMEM ivshmem
 * @{
 */

struct ivshmem_endpoint {
	u32 cspace[IVSHMEM_CFG_SIZE / sizeof(u32)];
	u32 ivpos;
	u32 state;
	u64 bar0_address;
	u64 bar4_address;
	struct pci_device *device;
	struct ivshmem_endpoint *remote;
	struct arch_pci_ivshmem arch;
};

int ivshmem_init(struct cell *cell, struct pci_device *device);
void ivshmem_exit(struct pci_device *device);
int ivshmem_update_msix(struct pci_device *device);
enum pci_access ivshmem_pci_cfg_write(struct pci_device *device,
				      unsigned int row, u32 mask, u32 value);
enum pci_access ivshmem_pci_cfg_read(struct pci_device *device, u16 address,
				     u32 *value);

bool ivshmem_is_msix_masked(struct ivshmem_endpoint *ive);

/**
 * Handle write to doorbell register.
 * @param ive		Ivshmem endpoint the write was performed on.
 */
void arch_ivshmem_write_doorbell(struct ivshmem_endpoint *ive);

/**
 * Update cached MSI-X state (if any) of the given ivshmem device.
 * @param device	The device to be updated.
 *
 * @return 0 on success, negative error code otherwise.
 */
int arch_ivshmem_update_msix(struct pci_device *device);

/** @} IVSHMEM */
#endif /* !_JAILHOUSE_IVSHMEM_H */
