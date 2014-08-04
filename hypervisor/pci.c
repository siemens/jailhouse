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

#include <jailhouse/acpi.h>
#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <jailhouse/utils.h>

#define PCI_CONFIG_HEADER_SIZE		0x40

struct acpi_mcfg_alloc {
	u64 base_addr;
	u16 segment_num;
	u8 start_bus;
	u8 end_bus;
	u32 reserved;
} __attribute__((packed));

struct acpi_mcfg_table {
	struct acpi_table_header header;
	u8 reserved[8];
	struct acpi_mcfg_alloc alloc_structs[];
} __attribute__((packed));

/* entry for PCI config space whitelist (granting access) */
struct pci_cfg_access {
	u32 reg_num; /** Register number (4-byte aligned) */
	u32 mask; /** Bit set: access allowed */
};

/* --- Whilelist for writing to PCI config space registers --- */
/* Type 1: Endpoints */
static const struct pci_cfg_access endpoint_write_access[] = {
	{ 0x04, 0xffffffff }, /* Command, Status */
	{ 0x0c, 0xff00ffff }, /* BIST, Latency Timer, Cacheline */
	{ 0x3c, 0x000000ff }, /* Int Line */
};
/* Type 2: Bridges */
static const struct pci_cfg_access bridge_write_access[] = {
	{ 0x04, 0xffffffff }, /* Command, Status */
	{ 0x0c, 0xff00ffff }, /* BIST, Latency Timer, Cacheline */
	{ 0x3c, 0xffff00ff }, /* Int Line, Bridge Control */
};

static void *pci_space;
static u64 pci_mmcfg_addr;
static u32 pci_mmcfg_size;
static u8 end_bus;

static void *pci_get_device_mmcfg_base(u16 bdf)
{
	return pci_space + ((unsigned long)bdf << 12);
}

/**
 * pci_read_config() - Read from PCI config space
 * @bdf:	16-bit bus/device/function ID of target
 * @address:	Config space access address
 * @size:	Access size (1, 2 or 4 bytes)
 *
 * Return: read value
 */
u32 pci_read_config(u16 bdf, u16 address, unsigned int size)
{
	void *mmcfg_addr = pci_get_device_mmcfg_base(bdf) + address;

	if (!pci_space || PCI_BUS(bdf) > end_bus)
		return arch_pci_read_config(bdf, address, size);

	if (size == 1)
		return mmio_read8(mmcfg_addr);
	else if (size == 2)
		return mmio_read16(mmcfg_addr);
	else
		return mmio_read32(mmcfg_addr);
}

/**
 * pci_write_config() - Write to PCI config space
 * @bdf:	16-bit bus/device/function ID of target
 * @address:	Config space access address
 * @value:	Value to be written
 * @size:	Access size (1, 2 or 4 bytes)
 */
void pci_write_config(u16 bdf, u16 address, u32 value, unsigned int size)
{
	void *mmcfg_addr = pci_get_device_mmcfg_base(bdf) + address;

	if (!pci_space || PCI_BUS(bdf) > end_bus)
		return arch_pci_write_config(bdf, address, value, size);

	if (size == 1)
		mmio_write8(mmcfg_addr, value);
	else if (size == 2)
		mmio_write16(mmcfg_addr, value);
	else
		mmio_write32(mmcfg_addr, value);
}

/**
 * pci_get_assigned_device() - Look up device owned by a cell
 * @cell:	Owning cell
 * @bdf:	16-bit bus/device/function ID
 *
 * Return: Pointer to owned PCI device or NULL.
 */
struct pci_device *pci_get_assigned_device(const struct cell *cell, u16 bdf)
{
	const struct jailhouse_pci_device *dev_info =
		jailhouse_cell_pci_devices(cell->config);
	u32 n;

	/* We iterate over the static device information to increase cache
	 * locality. */
	for (n = 0; n < cell->config->num_pci_devices; n++)
		if (dev_info[n].bdf == bdf)
			return cell->pci_devices[n].cell ?
				&cell->pci_devices[n] : NULL;

	return NULL;
}

/**
 * pci_find_capability() - Look up capability at given config space address
 * @device:	The device to be accessed
 * @address:	Config space access address
 *
 * Return: Corresponding capability structure or NULL if none found.
 */
static const struct jailhouse_pci_capability *
pci_find_capability(struct pci_device *device, u16 address)
{
	const struct jailhouse_pci_capability *cap =
		jailhouse_cell_pci_caps(device->cell->config) +
		device->info->caps_start;
	u32 n;

	for (n = 0; n < device->info->num_caps; n++, cap++)
		if (cap->start <= address && cap->start + cap->len > address)
			return cap;

	return NULL;
}

/**
 * pci_cfg_read_moderate() - Moderate config space read access
 * @device:	The device to be accessed; if NULL, access will be emulated,
 * 		returning a value of -1
 * @address:	Config space address
 * @size:	Access size (1, 2 or 4 bytes)
 * @value:	Pointer to buffer to receive the emulated value if
 * 		PCI_ACCESS_DONE is returned
 *
 * Return: PCI_ACCESS_PERFORM or PCI_ACCESS_DONE.
 */
enum pci_access pci_cfg_read_moderate(struct pci_device *device, u16 address,
				      unsigned int size, u32 *value)
{
	const struct jailhouse_pci_capability *cap;

	if (!device) {
		*value = -1;
		return PCI_ACCESS_DONE;
	}

	if (address < PCI_CONFIG_HEADER_SIZE)
		return PCI_ACCESS_PERFORM;

	cap = pci_find_capability(device, address);
	if (!cap)
		return PCI_ACCESS_PERFORM;

	// TODO: Emulate MSI/MSI-X etc.

	return PCI_ACCESS_PERFORM;
}

/**
 * pci_cfg_write_moderate() - Moderate config space write access
 * @device:	The device to be accessed; if NULL, access will be rejected
 * @address:	Config space address
 * @size:	Access size (1, 2 or 4 bytes)
 * @value:	Value to be written
 *
 * Return: PCI_ACCESS_REJECT, PCI_ACCESS_PERFORM or PCI_ACCESS_DONE.
 */
enum pci_access pci_cfg_write_moderate(struct pci_device *device, u16 address,
				       unsigned int size, u32 value)
{
	const struct jailhouse_pci_capability *cap;
	/* initialize list to work around wrong compiler warning */
	const struct pci_cfg_access *list = NULL;
	unsigned int n, bias_shift, len = 0;
	u32 mask;

	if (!device)
		return PCI_ACCESS_REJECT;

	if (address < PCI_CONFIG_HEADER_SIZE) {
		if (device->info->type == JAILHOUSE_PCI_TYPE_DEVICE) {
			list = endpoint_write_access;
			len = ARRAY_SIZE(endpoint_write_access);
		} else if (device->info->type == JAILHOUSE_PCI_TYPE_BRIDGE) {
			list = bridge_write_access;
			len = ARRAY_SIZE(bridge_write_access);
		}

		bias_shift = (address & 0x003) * 8;
		mask = BYTE_MASK(size);

		for (n = 0; n < len; n++) {
			if (list[n].reg_num == (address & 0xffc) &&
			    ((list[n].mask >> bias_shift) & mask) == mask)
				return PCI_ACCESS_PERFORM;
		}

		return PCI_ACCESS_REJECT;
	}

	cap = pci_find_capability(device, address);
	if (!cap || !(cap->flags & JAILHOUSE_PCICAPS_WRITE))
		return PCI_ACCESS_REJECT;

	return PCI_ACCESS_PERFORM;
}

/**
 * pci_init() - Initialization of PCI module
 *
 * Return: 0 - success, error code - if error.
 */
int pci_init(void)
{
	struct acpi_mcfg_table *mcfg;
	int err;

	err = pci_cell_init(&root_cell);
	if (err)
		return err;

	mcfg = (struct acpi_mcfg_table *)acpi_find_table("MCFG", NULL);
	if (!mcfg)
		return 0;

	if (mcfg->header.length !=
	    sizeof(struct acpi_mcfg_table) + sizeof(struct acpi_mcfg_alloc))
		return -EIO;

	pci_mmcfg_addr = mcfg->alloc_structs[0].base_addr;
	pci_mmcfg_size = (mcfg->alloc_structs[0].end_bus + 1) * 256 * 4096;
	pci_space = page_alloc(&remap_pool, pci_mmcfg_size / PAGE_SIZE);
	if (!pci_space)
		return -ENOMEM;

	end_bus = mcfg->alloc_structs[0].end_bus;

	return page_map_create(&hv_paging_structs,
			       mcfg->alloc_structs[0].base_addr,
			       pci_mmcfg_size, (unsigned long)pci_space,
			       PAGE_DEFAULT_FLAGS | PAGE_FLAG_UNCACHED,
			       PAGE_MAP_NON_COHERENT);
}

/**
 * pci_mmio_access_handler() - Handler for MMIO-accesses to PCI config space
 * @cell:	Request issuing cell
 * @is_write:	True if write access
 * @addr:	Address accessed
 * @value:	Pointer to value for reading/writing
 *
 * Return: 1 if handled successfully, 0 if unhandled, -1 on access error
 */
int pci_mmio_access_handler(const struct cell *cell, bool is_write,
			    u64 addr, u32 *value)
{
	u32 mmcfg_offset, reg_addr;
	struct pci_device *device;
	enum pci_access access;

	if (!pci_space || addr < pci_mmcfg_addr ||
	    addr >= (pci_mmcfg_addr + pci_mmcfg_size - 4))
		return 0;

	mmcfg_offset = addr - pci_mmcfg_addr;
	reg_addr = mmcfg_offset & 0xfff;
	device = pci_get_assigned_device(cell, mmcfg_offset >> 12);

	if (is_write) {
		access = pci_cfg_write_moderate(device, reg_addr, 4, *value);
		if (access == PCI_ACCESS_REJECT)
			goto invalid_access;
		if (access == PCI_ACCESS_PERFORM)
			mmio_write32(pci_space + mmcfg_offset, *value);
	} else {
		access = pci_cfg_read_moderate(device, reg_addr, 4, value);
		if (access == PCI_ACCESS_PERFORM)
			*value = mmio_read32(pci_space + mmcfg_offset);
	}

	return 1;

invalid_access:
	panic_printk("FATAL: Invalid PCI MMCONFIG write, device %02x:%02x.%x, "
		     "reg: %\n", PCI_BDF_PARAMS(mmcfg_offset >> 12), reg_addr);
	return -1;

}

static int pci_add_device(struct cell *cell, struct pci_device *device)
{
	printk("Adding PCI device %02x:%02x.%x to cell \"%s\"\n",
	       PCI_BDF_PARAMS(device->info->bdf), cell->config->name);
	return arch_pci_add_device(cell, device);
}

static void pci_remove_device(struct pci_device *device)
{
	printk("Removing PCI device %02x:%02x.%x from cell \"%s\"\n",
	       PCI_BDF_PARAMS(device->info->bdf), device->cell->config->name);
	arch_pci_remove_device(device);
}

int pci_cell_init(struct cell *cell)
{
	unsigned long array_size = PAGE_ALIGN(cell->config->num_pci_devices *
					      sizeof(struct pci_device));
	const struct jailhouse_pci_device *dev_infos =
		jailhouse_cell_pci_devices(cell->config);
	struct pci_device *device, *root_device;
	unsigned int ndev;
	int err;

	cell->pci_devices = page_alloc(&mem_pool, array_size / PAGE_SIZE);
	if (!cell->pci_devices)
		return -ENOMEM;

	/*
	 * We order device states in the same way as the static information
	 * so that we can use the index of the latter to find the former. For
	 * the other way around and for obtaining the owner cell, we use more
	 * handy pointers. The cell pointer also encodes active ownership.
	 */
	for (ndev = 0; ndev < cell->config->num_pci_devices; ndev++) {
		device = &cell->pci_devices[ndev];
		device->info = &dev_infos[ndev];

		root_device = pci_get_assigned_device(&root_cell,
						      dev_infos[ndev].bdf);
		if (root_device) {
			pci_remove_device(root_device);
			root_device->cell = NULL;
		}

		err = pci_add_device(cell, device);
		if (err) {
			pci_cell_exit(cell);
			return err;
		}

		device->cell = cell;
	}

	return 0;
}

static void pci_return_device_to_root_cell(struct pci_device *device)
{
	struct pci_device *root_device = root_cell.pci_devices;
	unsigned int n;

	for (n = 0; n < root_cell.config->num_pci_devices; n++, root_device++)
		if (root_device->info->domain == device->info->domain &&
		    root_device->info->bdf == device->info->bdf) {
			if (pci_add_device(&root_cell, root_device) < 0)
				printk("WARNING: Failed to re-assign PCI "
				       "device to root cell\n");
			else
				root_device->cell = &root_cell;
			break;
		}
}

void pci_cell_exit(struct cell *cell)
{
	unsigned long array_size = PAGE_ALIGN(cell->config->num_pci_devices *
					      sizeof(struct pci_device));
	unsigned int n;

	/*
	 * Do not destroy the root cell. We will shut down the complete
	 * hypervisor instead.
	 */
	if (cell == &root_cell)
		return;

	for (n = 0; n < cell->config->num_pci_devices; n++) {
		if (!cell->pci_devices[n].cell)
			continue;
		pci_remove_device(&cell->pci_devices[n]);
		pci_return_device_to_root_cell(&cell->pci_devices[n]);
	}

	page_free(&mem_pool, cell->pci_devices, array_size / PAGE_SIZE);
}
