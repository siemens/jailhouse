/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _JAILHOUSE_CELL_CONFIG_H
#define _JAILHOUSE_CELL_CONFIG_H

#define JAILHOUSE_CELL_NAME_MAXLEN	31

#define JAILHOUSE_CELL_PASSIVE_COMMREG	0x00000001

#define JAILHOUSE_CELL_DESC_SIGNATURE	"JAILCELL"

struct jailhouse_cell_desc {
	char signature[8];
	char name[JAILHOUSE_CELL_NAME_MAXLEN+1];
	__u32 flags;

	__u32 cpu_set_size;
	__u32 num_memory_regions;
	__u32 num_cache_regions;
	__u32 num_irqchips;
	__u32 pio_bitmap_size;
	__u32 num_pci_devices;
	__u32 num_pci_caps;
} __attribute__((packed));

#define JAILHOUSE_MEM_READ		0x0001
#define JAILHOUSE_MEM_WRITE		0x0002
#define JAILHOUSE_MEM_EXECUTE		0x0004
#define JAILHOUSE_MEM_DMA		0x0008
#define JAILHOUSE_MEM_IO		0x0010
#define JAILHOUSE_MEM_COMM_REGION	0x0020
#define JAILHOUSE_MEM_LOADABLE		0x0040
#define JAILHOUSE_MEM_ROOTSHARED	0x0080

#define JAILHOUSE_MEM_VALID_FLAGS	(JAILHOUSE_MEM_READ | \
					 JAILHOUSE_MEM_WRITE | \
					 JAILHOUSE_MEM_EXECUTE | \
					 JAILHOUSE_MEM_DMA | \
					 JAILHOUSE_MEM_IO | \
					 JAILHOUSE_MEM_COMM_REGION | \
					 JAILHOUSE_MEM_LOADABLE | \
					 JAILHOUSE_MEM_ROOTSHARED)

struct jailhouse_memory {
	__u64 phys_start;
	__u64 virt_start;
	__u64 size;
	__u64 flags;
} __attribute__((packed));

#define JAILHOUSE_CACHE_L3_CODE		0x01
#define JAILHOUSE_CACHE_L3_DATA		0x02
#define JAILHOUSE_CACHE_L3		(JAILHOUSE_CACHE_L3_CODE | \
					 JAILHOUSE_CACHE_L3_DATA)

#define JAILHOUSE_CACHE_ROOTSHARED	0x0001

struct jailhouse_cache {
	__u32 start;
	__u32 size;
	__u8 type;
	__u8 padding;
	__u16 flags;
} __attribute__((packed));

struct jailhouse_irqchip {
	__u64 address;
	__u64 id;
	__u64 pin_bitmap;
} __attribute__((packed));

#define JAILHOUSE_PCI_TYPE_DEVICE	0x01
#define JAILHOUSE_PCI_TYPE_BRIDGE	0x02
#define JAILHOUSE_PCI_TYPE_IVSHMEM	0x03

struct jailhouse_pci_device {
	__u8 type;
	__u8 iommu;
	__u16 domain;
	__u16 bdf;
	__u32 bar_mask[6];
	__u16 caps_start;
	__u16 num_caps;
	__u8 num_msi_vectors;
	__u8 msi_64bits;
	__u16 num_msix_vectors;
	__u16 msix_region_size;
	__u64 msix_address;
	/** used to refer to memory in virtual PCI devices */
	__u32 shmem_region;
} __attribute__((packed));

#define JAILHOUSE_PCI_EXT_CAP		0x8000

#define JAILHOUSE_PCICAPS_WRITE		0x0001

struct jailhouse_pci_capability {
	__u16 id;
	__u16 start;
	__u16 len;
	__u16 flags;
} __attribute__((packed));

#define JAILHOUSE_MAX_IOMMU_UNITS	8

#define JAILHOUSE_SYSTEM_SIGNATURE	"JAILSYST"

struct jailhouse_system {
	char signature[8];
	struct jailhouse_memory hypervisor_memory;
	struct jailhouse_memory debug_uart;
	union {
		struct {
			__u64 mmconfig_base;
			__u8 mmconfig_end_bus;
			__u8 padding[5];
			__u16 pm_timer_address;
			__u64 iommu_base[JAILHOUSE_MAX_IOMMU_UNITS];
		} __attribute__((packed)) x86;
	} __attribute__((packed)) platform_info;
	__u32 device_limit;
	__u32 interrupt_limit;
	struct jailhouse_cell_desc root_cell;
} __attribute__((packed));

static inline __u32
jailhouse_cell_config_size(struct jailhouse_cell_desc *cell)
{
	return sizeof(struct jailhouse_cell_desc) +
		cell->cpu_set_size +
		cell->num_memory_regions * sizeof(struct jailhouse_memory) +
		cell->num_cache_regions * sizeof(struct jailhouse_cache) +
		cell->num_irqchips * sizeof(struct jailhouse_irqchip) +
		cell->pio_bitmap_size +
		cell->num_pci_devices * sizeof(struct jailhouse_pci_device) +
		cell->num_pci_caps * sizeof(struct jailhouse_pci_capability);
}

static inline __u32
jailhouse_system_config_size(struct jailhouse_system *system)
{
	return sizeof(*system) - sizeof(system->root_cell) +
		jailhouse_cell_config_size(&system->root_cell);
}

static inline const unsigned long *
jailhouse_cell_cpu_set(const struct jailhouse_cell_desc *cell)
{
	return (const unsigned long *)((const void *)cell +
		sizeof(struct jailhouse_cell_desc));
}

static inline const struct jailhouse_memory *
jailhouse_cell_mem_regions(const struct jailhouse_cell_desc *cell)
{
	return (const struct jailhouse_memory *)
		((void *)jailhouse_cell_cpu_set(cell) + cell->cpu_set_size);
}

static inline const struct jailhouse_cache *
jailhouse_cell_cache_regions(const struct jailhouse_cell_desc *cell)
{
	return (const struct jailhouse_cache *)
		((void *)jailhouse_cell_mem_regions(cell) +
		 cell->num_memory_regions * sizeof(struct jailhouse_memory));
}

static inline const struct jailhouse_irqchip *
jailhouse_cell_irqchips(const struct jailhouse_cell_desc *cell)
{
	return (const struct jailhouse_irqchip *)
		((void *)jailhouse_cell_cache_regions(cell) +
		 cell->num_cache_regions * sizeof(struct jailhouse_cache));
}

static inline const __u8 *
jailhouse_cell_pio_bitmap(const struct jailhouse_cell_desc *cell)
{
	return (const __u8 *)((void *)jailhouse_cell_irqchips(cell) +
		cell->num_irqchips * sizeof(struct jailhouse_irqchip));
}

static inline const struct jailhouse_pci_device *
jailhouse_cell_pci_devices(const struct jailhouse_cell_desc *cell)
{
	return (const struct jailhouse_pci_device *)
		((void *)jailhouse_cell_pio_bitmap(cell) +
		 cell->pio_bitmap_size);
}

static inline const struct jailhouse_pci_capability *
jailhouse_cell_pci_caps(const struct jailhouse_cell_desc *cell)
{
	return (const struct jailhouse_pci_capability *)
		((void *)jailhouse_cell_pci_devices(cell) +
		 cell->num_pci_devices * sizeof(struct jailhouse_pci_device));
}

#endif /* !_JAILHOUSE_CELL_CONFIG_H */
