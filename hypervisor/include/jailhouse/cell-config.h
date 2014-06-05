/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_CELL_CONFIG_H
#define _JAILHOUSE_CELL_CONFIG_H

#define JAILHOUSE_CELL_NAME_MAXLEN	31

#define JAILHOUSE_CELL_PASSIVE_COMMREG	0x00000001

struct jailhouse_cell_desc {
	char name[JAILHOUSE_CELL_NAME_MAXLEN+1];
	__u32 flags;

	__u32 cpu_set_size;
	__u32 num_memory_regions;
	__u32 num_irq_lines;
	__u32 pio_bitmap_size;
	__u32 num_pci_devices;
} __attribute__((packed));

#define JAILHOUSE_MEM_READ		0x0001
#define JAILHOUSE_MEM_WRITE		0x0002
#define JAILHOUSE_MEM_EXECUTE		0x0004
#define JAILHOUSE_MEM_DMA		0x0008
#define JAILHOUSE_MEM_COMM_REGION	0x0010
#define JAILHOUSE_MEM_LOADABLE		0x0020

#define JAILHOUSE_MEM_VALID_FLAGS	(JAILHOUSE_MEM_READ | \
					 JAILHOUSE_MEM_WRITE | \
					 JAILHOUSE_MEM_EXECUTE | \
					 JAILHOUSE_MEM_DMA | \
					 JAILHOUSE_MEM_COMM_REGION | \
					 JAILHOUSE_MEM_LOADABLE)

struct jailhouse_memory {
	__u64 phys_start;
	__u64 virt_start;
	__u64 size;
	__u64 flags;
} __attribute__((packed));

struct jailhouse_irq_line {
	__u32 num;
	__u32 irqchip;
} __attribute__((packed));

#define JAILHOUSE_PCI_TYPE_DEVICE	0x01
#define JAILHOUSE_PCI_TYPE_BRIDGE	0x02

struct jailhouse_pci_device {
	__u32 type;
	__u16 domain;
	__u8 bus;
	__u8 devfn;
} __attribute__((packed));

struct jailhouse_system {
	struct jailhouse_memory hypervisor_memory;
	struct jailhouse_memory config_memory;
	struct jailhouse_cell_desc system;
} __attribute__((packed));

static inline __u32
jailhouse_cell_config_size(struct jailhouse_cell_desc *cell)
{
	return sizeof(struct jailhouse_cell_desc) +
		cell->cpu_set_size +
		cell->num_memory_regions * sizeof(struct jailhouse_memory) +
		cell->num_irq_lines * sizeof(struct jailhouse_irq_line) +
		cell->pio_bitmap_size +
		cell->num_pci_devices * sizeof(struct jailhouse_pci_device);
}

static inline __u32
jailhouse_system_config_size(struct jailhouse_system *system)
{
	return sizeof(system->hypervisor_memory) +
		sizeof(system->config_memory) +
		jailhouse_cell_config_size(&system->system);
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
	return (const struct jailhouse_memory *)((void *)cell +
		sizeof(struct jailhouse_cell_desc) + cell->cpu_set_size);
}

static inline const __u8 *
jailhouse_cell_pio_bitmap(const struct jailhouse_cell_desc *cell)
{
	return (const __u8 *)((void *)cell +
		sizeof(struct jailhouse_cell_desc) + cell->cpu_set_size +
		cell->num_memory_regions * sizeof(struct jailhouse_memory) +
		cell->num_irq_lines * sizeof(struct jailhouse_irq_line));
}

static inline const struct jailhouse_pci_device *
jailhouse_cell_pci_devices(const struct jailhouse_cell_desc *cell)
{
	return (const struct jailhouse_pci_device *)((void *)cell +
		sizeof(struct jailhouse_cell_desc) + cell->cpu_set_size +
		cell->num_memory_regions * sizeof(struct jailhouse_memory) +
		cell->num_irq_lines * sizeof(struct jailhouse_irq_line) +
		cell->pio_bitmap_size);
}

#endif /* !_JAILHOUSE_CELL_CONFIG_H */
