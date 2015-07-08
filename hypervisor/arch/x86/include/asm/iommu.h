/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_IOMMU_H
#define _JAILHOUSE_ASM_IOMMU_H

#include <jailhouse/entry.h>
#include <jailhouse/cell.h>
#include <jailhouse/cell-config.h>
#include <jailhouse/pci.h>
#include <jailhouse/types.h>
#include <asm/apic.h>
#include <asm/percpu.h>

unsigned int iommu_count_units(void);

int iommu_init(void);

int iommu_cell_init(struct cell *cell);
int iommu_map_memory_region(struct cell *cell,
			    const struct jailhouse_memory *mem);
int iommu_unmap_memory_region(struct cell *cell,
			      const struct jailhouse_memory *mem);
int iommu_add_pci_device(struct cell *cell, struct pci_device *device);
void iommu_remove_pci_device(struct pci_device *device);

struct apic_irq_message iommu_get_remapped_root_int(unsigned int iommu,
						    u16 device_id,
						    unsigned int vector,
						    unsigned int remap_index);
int iommu_map_interrupt(struct cell *cell,
			u16 device_id,
			unsigned int vector,
			struct apic_irq_message irq_msg);

void iommu_cell_exit(struct cell *cell);

void iommu_config_commit(struct cell *cell_added_removed);

void iommu_shutdown(void);

void iommu_check_pending_faults(void);

int iommu_mmio_access_handler(bool is_write, u64 addr, u32 *value);

bool iommu_cell_emulates_ir(struct cell *cell);

#endif
