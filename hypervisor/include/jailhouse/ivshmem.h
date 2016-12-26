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
#include <asm/spinlock.h>

#define IVSHMEM_CFG_MSIX_CAP	0x50
#define IVSHMEM_CFG_SIZE	(IVSHMEM_CFG_MSIX_CAP + 12)

#define IVSHMEM_INTX_ENABLE	0x1

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
	const struct jailhouse_memory *shmem;
	struct ivshmem_endpoint *remote;
	spinlock_t remote_lock;
	struct arch_pci_ivshmem arch;
	u32 intx_ctrl_reg;
};

int ivshmem_init(struct cell *cell, struct pci_device *device);
void ivshmem_reset(struct pci_device *device);
void ivshmem_exit(struct pci_device *device);
int ivshmem_update_msix(struct pci_device *device);
enum pci_access ivshmem_pci_cfg_write(struct pci_device *device,
				      unsigned int row, u32 mask, u32 value);
enum pci_access ivshmem_pci_cfg_read(struct pci_device *device, u16 address,
				     u32 *value);

bool ivshmem_is_msix_masked(struct ivshmem_endpoint *ive);

/**
 * Trigger interrupt on ivshmem endpoint.
 * @param ive		Ivshmem endpoint the interrupt should be raised at.
 */
void arch_ivshmem_trigger_interrupt(struct ivshmem_endpoint *ive);

/**
 * Update cached MSI-X state (if any) of the given ivshmem device.
 * @param device	The device to be updated.
 *
 * @return 0 on success, negative error code otherwise.
 */
int arch_ivshmem_update_msix(struct pci_device *device);

/**
 * Update cached INTx state (if any) of the given ivshmem device.
 * @param ive		Ivshmem endpoint to be updated.
 */
void arch_ivshmem_update_intx(struct ivshmem_endpoint *ive);

/** @} IVSHMEM */
#endif /* !_JAILHOUSE_IVSHMEM_H */
