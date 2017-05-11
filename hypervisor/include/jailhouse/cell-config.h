/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2016
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

/* Incremented on any layout or semantic change of system or cell config. */
#define JAILHOUSE_CONFIG_REVISION	5

#define JAILHOUSE_CELL_NAME_MAXLEN	31

#define JAILHOUSE_CELL_PASSIVE_COMMREG	0x00000001
#define JAILHOUSE_CELL_DEBUG_CONSOLE	0x00000002

#define JAILHOUSE_CELL_DESC_SIGNATURE	"JHCELL"

/**
 * The jailhouse cell configuration.
 *
 * @note Keep Config._HEADER_FORMAT in jailhouse-cell-linux in sync with this
 * structure.
 */
struct jailhouse_cell_desc {
	char signature[6];
	__u16 revision;

	char name[JAILHOUSE_CELL_NAME_MAXLEN+1];
	__u32 id; /* set by the driver */
	__u32 flags;

	__u32 cpu_set_size;
	__u32 num_memory_regions;
	__u32 num_cache_regions;
	__u32 num_irqchips;
	__u32 pio_bitmap_size;
	__u32 num_pci_devices;
	__u32 num_pci_caps;

	__u32 vpci_irq_base;

	__u64 cpu_reset_address;
} __attribute__((packed));

#define JAILHOUSE_MEM_READ		0x0001
#define JAILHOUSE_MEM_WRITE		0x0002
#define JAILHOUSE_MEM_EXECUTE		0x0004
#define JAILHOUSE_MEM_DMA		0x0008
#define JAILHOUSE_MEM_IO		0x0010
#define JAILHOUSE_MEM_COMM_REGION	0x0020
#define JAILHOUSE_MEM_LOADABLE		0x0040
#define JAILHOUSE_MEM_ROOTSHARED	0x0080
#define JAILHOUSE_MEM_IO_UNALIGNED	0x0100
#define JAILHOUSE_MEM_IO_WIDTH_SHIFT	16 /* uses bits 16..19 */
#define JAILHOUSE_MEM_IO_8		(1 << JAILHOUSE_MEM_IO_WIDTH_SHIFT)
#define JAILHOUSE_MEM_IO_16		(2 << JAILHOUSE_MEM_IO_WIDTH_SHIFT)
#define JAILHOUSE_MEM_IO_32		(4 << JAILHOUSE_MEM_IO_WIDTH_SHIFT)
#define JAILHOUSE_MEM_IO_64		(8 << JAILHOUSE_MEM_IO_WIDTH_SHIFT)

struct jailhouse_memory {
	__u64 phys_start;
	__u64 virt_start;
	__u64 size;
	__u64 flags;
} __attribute__((packed));

#define JAILHOUSE_MEMORY_IS_SUBPAGE(mem)	\
	((mem)->virt_start & ~PAGE_MASK || (mem)->size & ~PAGE_MASK)

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
	__u32 id;
	__u32 pin_base;
	__u32 pin_bitmap[4];
} __attribute__((packed));

#define JAILHOUSE_PCI_TYPE_DEVICE	0x01
#define JAILHOUSE_PCI_TYPE_BRIDGE	0x02
#define JAILHOUSE_PCI_TYPE_IVSHMEM	0x03

#define JAILHOUSE_SHMEM_PROTO_UNDEFINED	0x0000
#define JAILHOUSE_SHMEM_PROTO_VETH	0x0100
#define JAILHOUSE_SHMEM_PROTO_CUSTOM	0x8000	/* 0x80xx..0xffxx */

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
	/** Memory region index of virtual shared memory device. */
	__u32 shmem_region;
	/** PCI subclass and interface ID of virtual shared memory device. */
	__u16 shmem_protocol;
	__u8 padding[2];
} __attribute__((packed));

#define JAILHOUSE_PCI_EXT_CAP		0x8000

#define JAILHOUSE_PCICAPS_WRITE		0x0001

struct jailhouse_pci_capability {
	__u16 id;
	__u16 start;
	__u16 len;
	__u16 flags;
} __attribute__((packed));

#define JAILHOUSE_APIC_MODE_AUTO	0
#define JAILHOUSE_APIC_MODE_XAPIC	1
#define JAILHOUSE_APIC_MODE_X2APIC	2

#define JAILHOUSE_MAX_IOMMU_UNITS	8

struct jailhouse_iommu {
	__u64 base;
	__u32 size;
	__u16 amd_bdf;
	__u8 amd_base_cap;
	__u8 amd_msi_cap;
	__u32 amd_features;
} __attribute__((packed));

/* Bits 0..3 are used to select the particular driver */
#define JAILHOUSE_CON1_TYPE_NONE	0x0000
#define JAILHOUSE_CON1_TYPE_VGA		0x0001
#define JAILHOUSE_CON1_TYPE_8250	0x0002
#define JAILHOUSE_CON1_TYPE_PL011	0x0003
#define JAILHOUSE_CON1_TYPE_XUARTPS	0x0004
#define JAILHOUSE_CON1_TYPE_MASK	0x000f

#define CON1_TYPE(flags) ((flags) & JAILHOUSE_CON1_TYPE_MASK)

/* Bits 4 is used to select PIO (cleared) or MMIO (set) access */
#define JAILHOUSE_CON1_ACCESS_PIO	0x0000
#define JAILHOUSE_CON1_ACCESS_MMIO	0x0010

#define CON1_IS_MMIO(flags) ((flags) & JAILHOUSE_CON1_ACCESS_MMIO)

/* Bits 5 is used to select 1 (cleared) or 4-bytes (set) register distance.
 * 1 byte implied 8-bit access, 4 bytes 32-bit access. */
#define JAILHOUSE_CON1_REGDIST_1	0x0000
#define JAILHOUSE_CON1_REGDIST_4	0x0020

#define CON1_USES_REGDIST_1(flags) (((flags) & JAILHOUSE_CON1_REGDIST_4) == 0)

/* Bits 8..11 are used to select the second console driver */
#define JAILHOUSE_CON2_TYPE_ROOTPAGE	0x0100
#define JAILHOUSE_CON2_TYPE_MASK	0x0f00

#define CON2_TYPE(flags) ((flags) & JAILHOUSE_CON2_TYPE_MASK)

struct jailhouse_debug_console {
	__u64 address;
	__u32 size;
	__u32 flags;
	__u32 divider;
	__u32 gate_nr;
	__u64 clock_reg;
} __attribute__((packed));

#define JAILHOUSE_SYSTEM_SIGNATURE	"JHSYST"

/**
 * General descriptor of the system.
 */
struct jailhouse_system {
	char signature[6];
	__u16 revision;

	/** Jailhouse's location in memory */
	struct jailhouse_memory hypervisor_memory;
	struct jailhouse_debug_console debug_console;
	struct {
		__u64 pci_mmconfig_base;
		__u8 pci_mmconfig_end_bus;
		__u8 pci_is_virtual;
		union {
			struct {
				__u16 pm_timer_address;
				__u32 vtd_interrupt_limit;
				__u8 apic_mode;
				__u8 padding[3];
				struct jailhouse_iommu
					iommu_units[JAILHOUSE_MAX_IOMMU_UNITS];
			} __attribute__((packed)) x86;
			struct {
				u8 maintenance_irq;
				u8 padding;
				u64 gicd_base;
				u64 gicc_base;
				u64 gich_base;
				u64 gicv_base;
				u64 gicr_base;
			} __attribute__((packed)) arm;
		} __attribute__((packed));
	} __attribute__((packed)) platform_info;
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
