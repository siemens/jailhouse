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

struct jailhouse_cell_desc {
	char name[JAILHOUSE_CELL_NAME_MAXLEN+1];

	__u32 cpu_set_size;
	__u32 num_memory_regions;
	__u32 num_irq_lines;
	__u32 pio_bitmap_size;

	__u32 num_pci_devices;

	__u32 padding[3];
};

#define JAILHOUSE_MEM_READ		0x0001
#define JAILHOUSE_MEM_WRITE		0x0002
#define JAILHOUSE_MEM_EXECUTE		0x0004
#define JAILHOUSE_MEM_DMA		0x0008

#define JAILHOUSE_MEM_VALID_FLAGS	(JAILHOUSE_MEM_READ | \
					 JAILHOUSE_MEM_WRITE | \
					 JAILHOUSE_MEM_EXECUTE | \
					 JAILHOUSE_MEM_DMA)

struct jailhouse_memory {
	__u64 phys_start;
	__u64 virt_start;
	__u64 size;
	__u64 access_flags;
};

struct jailhouse_irq_line {
	__u32 num;
	__u32 irqchip;
};


struct jailhouse_pci_bridge {
	// TODO
	__u32 num_device;
};

#define JAILHOUSE_PCI_TYPE_DEVICE	0x01
#define JAILHOUSE_PCI_TYPE_BRIDGE	0x02

struct jailhouse_pci_device {
	// TODO
	__u32 type;
	__u16 domain;
	__u8 bus;
	__u8 devfn;
} __attribute__((packed));


struct jailhouse_system {
	struct jailhouse_memory hypervisor_memory;
	struct jailhouse_memory config_memory;
	struct jailhouse_cell_desc system;
};

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

#endif /* !_JAILHOUSE_CELL_CONFIG_H */
