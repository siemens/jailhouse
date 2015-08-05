/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014, 2015
 *
 * Authors:
 *  Ivan Kolchin <ivan.kolchin@siemens.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <jailhouse/utils.h>

#define MSIX_VECTOR_CTRL_DWORD		3

#define for_each_configured_pci_device(dev, cell)			\
	for ((dev) = (cell)->pci_devices;				\
	     (dev) - (cell)->pci_devices < (cell)->config->num_pci_devices; \
	     (dev)++)

#define for_each_pci_cap(cap, dev, counter)				\
	for ((cap) = jailhouse_cell_pci_caps((dev)->cell->config) +	\
		(dev)->info->caps_start, (counter) = 0;			\
	     (counter) < (dev)->info->num_caps;				\
	     (cap)++, (counter)++)

/* entry for PCI config space access control */
struct pci_cfg_control {
	enum {
		PCI_CONFIG_DENY,
		PCI_CONFIG_ALLOW,
		PCI_CONFIG_RDONLY,
	} type;   /* Access type */
	u32 mask; /* Bit set: access type applies; bit cleared: deny access */
};

/* --- Access control for writing to PCI config space registers --- */
/* Type 1: Endpoints */
static const struct pci_cfg_control endpoint_write[PCI_CONFIG_HEADER_SIZE] = {
	[0x04/4] = {PCI_CONFIG_ALLOW,  0xffffffff}, /* Command, Status */
	[0x0c/4] = {PCI_CONFIG_ALLOW,  0xff00ffff}, /* BIST, Lat., Cacheline */
	[0x30/4] = {PCI_CONFIG_RDONLY, 0xffffffff}, /* ROM BAR */
	[0x3c/4] = {PCI_CONFIG_ALLOW,  0x000000ff}, /* Int Line */
};

/* Type 2: Bridges
 * Note: Ignore limit/base reprogramming attempts because the root cell will
 *       perform them on bus rescans. */
static const struct pci_cfg_control bridge_write[PCI_CONFIG_HEADER_SIZE] = {
	[0x04/4] = {PCI_CONFIG_ALLOW,  0xffffffff}, /* Command, Status */
	[0x0c/4] = {PCI_CONFIG_ALLOW,  0xff00ffff}, /* BIST, Lat., Cacheline */
	[0x1c/4] = {PCI_CONFIG_RDONLY, 0x0000ffff}, /* I/O Limit & Base */
	[0x20/4 ...      /* Memory Limit/Base, Prefetch Memory Limit/Base, */
	 0x30/4] = {PCI_CONFIG_RDONLY, 0xffffffff}, /* I/O Limit & Base */
	[0x3c/4] = {PCI_CONFIG_ALLOW,  0xffff00ff}, /* Int Line, Bridge Ctrl */
};

static void *pci_space;
static u64 mmcfg_start, mmcfg_end;
static u8 end_bus;

unsigned int pci_mmio_count_regions(struct cell *cell)
{
	const struct jailhouse_pci_device *dev_infos =
		jailhouse_cell_pci_devices(cell->config);
	unsigned int n, regions = 0;

	for (n = 0; n < cell->config->num_pci_devices; n++)
		if (dev_infos[n].type == JAILHOUSE_PCI_TYPE_IVSHMEM)
			regions += PCI_IVSHMEM_NUM_MMIO_REGIONS;

	return regions;
}

static void *pci_get_device_mmcfg_base(u16 bdf)
{
	return pci_space + ((unsigned long)bdf << 12);
}

/**
 * Read from PCI config space.
 * @param bdf		16-bit bus/device/function ID of target.
 * @param address	Config space access address.
 * @param size		Access size (1, 2 or 4 bytes).
 *
 * @return Read value.
 *
 * @see pci_write_config
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
 * Write to PCI config space.
 * @param bdf		16-bit bus/device/function ID of target.
 * @param address	Config space access address.
 * @param value		Value to be written.
 * @param size		Access size (1, 2 or 4 bytes).
 *
 * @see pci_read_config
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
 * Look up device owned by a cell.
 * @param[in] cell	Owning cell.
 * @param bdf		16-bit bus/device/function ID.
 *
 * @return Pointer to owned PCI device or NULL.
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
 * Look up capability at given config space address.
 * @param device	The device to be accessed.
 * @param address	Config space access address.
 *
 * @return Corresponding capability structure or NULL if none found.
 *
 * @private
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
 * Moderate config space read access.
 * @param device	The device to be accessed. If NULL, access will be
 * 			emulated, returning a value of -1.
 * @param address	Config space address.
 * @param size		Access size (1, 2 or 4 bytes).
 * @param value		Pointer to buffer to receive the emulated value if
 * 			PCI_ACCESS_DONE is returned.
 *
 * @return PCI_ACCESS_PERFORM or PCI_ACCESS_DONE.
 *
 * @see pci_cfg_write_moderate
 */
enum pci_access pci_cfg_read_moderate(struct pci_device *device, u16 address,
				      unsigned int size, u32 *value)
{
	const struct jailhouse_pci_capability *cap;
	unsigned int bar_no, cap_offs;

	if (!device) {
		*value = -1;
		return PCI_ACCESS_DONE;
	}

	/* Emulate BARs for physical and virtual devices */
	if (device->info->type != JAILHOUSE_PCI_TYPE_BRIDGE) {
		/* Emulate BAR access, always returning the shadow value. */
		if (address >= PCI_CFG_BAR && address <= PCI_CFG_BAR_END) {
			bar_no = (address - PCI_CFG_BAR) / 4;
			*value = device->bar[bar_no] >> ((address % 4) * 8);
			return PCI_ACCESS_DONE;
		}

		/* We do not expose ROMs. */
		if (address >= PCI_CFG_ROMBAR && address < PCI_CFG_CAPS) {
			*value = 0;
			return PCI_ACCESS_DONE;
		}
	}

	if (device->info->type == JAILHOUSE_PCI_TYPE_IVSHMEM)
		return pci_ivshmem_cfg_read(device, address, value);

	if (address < PCI_CONFIG_HEADER_SIZE)
		return PCI_ACCESS_PERFORM;

	cap = pci_find_capability(device, address);
	if (!cap)
		return PCI_ACCESS_PERFORM;

	cap_offs = address - cap->start;
	if (cap->id == PCI_CAP_MSI && cap_offs >= 4 &&
	    (cap_offs < 10 || (device->info->msi_64bits && cap_offs < 14))) {
		*value = device->msi_registers.raw[cap_offs / 4] >>
			((cap_offs % 4) * 8);
		return PCI_ACCESS_DONE;
	}

	return PCI_ACCESS_PERFORM;
}

static int pci_update_msix(struct pci_device *device,
			   const struct jailhouse_pci_capability *cap)
{
	unsigned int n;
	int result;

	for (n = 0; n < device->info->num_msix_vectors; n++) {
		result = arch_pci_update_msix_vector(device, n);
		if (result < 0)
			return result;
	}
	return 0;
}

/**
 * Moderate config space write access.
 * @param device	The device to be accessed. If NULL, access will be
 * 			rejected.
 * @param address	Config space address.
 * @param size		Access size (1, 2 or 4 bytes).
 * @param value		Value to be written.
 *
 * @return PCI_ACCESS_REJECT, PCI_ACCESS_PERFORM or PCI_ACCESS_DONE.
 *
 * @see pci_cfg_read_moderate
 */
enum pci_access pci_cfg_write_moderate(struct pci_device *device, u16 address,
				       unsigned int size, u32 value)
{
	const struct jailhouse_pci_capability *cap;
	/* initialize list to work around wrong compiler warning */
	unsigned int bias_shift = (address % 4) * 8;
	u32 mask = BYTE_MASK(size) << bias_shift;
	struct pci_cfg_control cfg_control;
	unsigned int bar_no, cap_offs;

	if (!device)
		return PCI_ACCESS_REJECT;

	value <<= bias_shift;

	/* Emulate BARs for physical and virtual devices */
	if (device->info->type != JAILHOUSE_PCI_TYPE_BRIDGE &&
	    address >= PCI_CFG_BAR && address <= PCI_CFG_BAR_END) {
		bar_no = (address - PCI_CFG_BAR) / 4;
		mask &= device->info->bar_mask[bar_no];
		device->bar[bar_no] &= ~mask;
		device->bar[bar_no] |= value & mask;
		return PCI_ACCESS_DONE;
	}

	if (address < PCI_CONFIG_HEADER_SIZE) {
		if (device->info->type == JAILHOUSE_PCI_TYPE_BRIDGE)
			cfg_control = bridge_write[address / 4];
		else /* physical or virtual device */
			cfg_control = endpoint_write[address / 4];

		if ((cfg_control.mask & mask) != mask)
			return PCI_ACCESS_REJECT;

		switch (cfg_control.type) {
		case PCI_CONFIG_ALLOW:
			if (device->info->type == JAILHOUSE_PCI_TYPE_IVSHMEM)
				return pci_ivshmem_cfg_write(device,
						address / 4, mask, value);
			return PCI_ACCESS_PERFORM;
		case PCI_CONFIG_RDONLY:
			return PCI_ACCESS_DONE;
		default:
			return PCI_ACCESS_REJECT;
		}
	}

	if (device->info->type == JAILHOUSE_PCI_TYPE_IVSHMEM)
		return pci_ivshmem_cfg_write(device, address / 4, mask, value);

	cap = pci_find_capability(device, address);
	if (!cap || !(cap->flags & JAILHOUSE_PCICAPS_WRITE))
		return PCI_ACCESS_REJECT;

	cap_offs = address - cap->start;
	if (cap->id == PCI_CAP_MSI &&
	    (cap_offs < 10 || (device->info->msi_64bits && cap_offs < 14))) {
		device->msi_registers.raw[cap_offs / 4] &= ~mask;
		device->msi_registers.raw[cap_offs / 4] |= value;

		if (arch_pci_update_msi(device, cap) < 0)
			return PCI_ACCESS_REJECT;

		/*
		 * Address and data words are emulated, the control word is
		 * written as-is.
		 */
		if (cap_offs >= 4)
			return PCI_ACCESS_DONE;
	} else if (cap->id == PCI_CAP_MSIX && cap_offs < 4) {
		device->msix_registers.raw &= ~mask;
		device->msix_registers.raw |= value;

		if (pci_update_msix(device, cap) < 0)
			return PCI_ACCESS_REJECT;
	}

	return PCI_ACCESS_PERFORM;
}

/**
 * Initialization of PCI subsystem.
 *
 * @return 0 on success, negative error code otherwise.
 */
int pci_init(void)
{
	unsigned int mmcfg_size;
	int err;

	err = pci_cell_init(&root_cell);
	if (err)
		return err;

	mmcfg_start = system_config->platform_info.x86.mmconfig_base;
	if (mmcfg_start == 0)
		return 0;

	end_bus = system_config->platform_info.x86.mmconfig_end_bus;
	mmcfg_size = (end_bus + 1) * 256 * 4096;
	mmcfg_end = mmcfg_start + mmcfg_size - 4;

	pci_space = page_alloc(&remap_pool, mmcfg_size / PAGE_SIZE);
	if (!pci_space)
		return trace_error(-ENOMEM);

	return paging_create(&hv_paging_structs, mmcfg_start, mmcfg_size,
			     (unsigned long)pci_space,
			     PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE,
			     PAGING_NON_COHERENT);
}

static int pci_msix_access_handler(const struct cell *cell, bool is_write,
				   u64 addr, u32 *value)
{
	unsigned int dword = (addr % sizeof(union pci_msix_vector)) >> 2;
	struct pci_device *device = cell->msix_device_list;
	unsigned int index;
	u64 offs;

	while (device) {
		if (addr >= device->info->msix_address &&
		    addr < device->info->msix_address +
			   device->info->msix_region_size)
			goto found;
		device = device->next_msix_device;
	}
	return 0;

found:
	/* access must be DWORD-aligned */
	if (addr & 0x3)
		goto invalid_access;

	offs = addr - device->info->msix_address;
	index = offs / sizeof(union pci_msix_vector);

	if (is_write) {
		/*
		 * The PBA may share a page with the MSI-X table. Writing to
		 * PBA entries is undefined. We declare it as invalid.
		 */
		if (index >= device->info->num_msix_vectors)
			goto invalid_access;

		device->msix_vectors[index].raw[dword] = *value;
		if (arch_pci_update_msix_vector(device, index) < 0)
			goto invalid_access;

		if (dword == MSIX_VECTOR_CTRL_DWORD)
			mmio_write32(&device->msix_table[index].raw[dword],
				     *value);
	} else {
		if (index >= device->info->num_msix_vectors ||
		    dword == MSIX_VECTOR_CTRL_DWORD)
			*value =
			    mmio_read32(((void *)device->msix_table) + offs);
		else
			*value = device->msix_vectors[index].raw[dword];
	}
	return 1;

invalid_access:
	panic_printk("FATAL: Invalid PCI MSI-X table/PBA access, device "
		     "%02x:%02x.%x\n", PCI_BDF_PARAMS(device->info->bdf));
	return -1;
}

/**
 * Handler for MMIO-accesses to PCI config space.
 * @param cell		Request issuing cell.
 * @param is_write	True if write access.
 * @param addr		Address accessed.
 * @param value		Pointer to value for reading/writing.
 *
 * @return 1 if handled successfully, 0 if unhandled, -1 on access error.
 */
int pci_mmio_access_handler(const struct cell *cell, bool is_write,
			    u64 addr, u32 *value)
{
	u32 mmcfg_offset, reg_addr;
	struct pci_device *device;
	enum pci_access access;

	if (!pci_space || addr < mmcfg_start || addr > mmcfg_end)
		return pci_msix_access_handler(cell, is_write, addr, value);

	mmcfg_offset = addr - mmcfg_start;
	reg_addr = mmcfg_offset & 0xfff;
	/* access must be DWORD-aligned */
	if (reg_addr & 0x3)
		goto invalid_access;

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

/**
 * Retrieve number of enabled MSI vector of a device.
 * @param device	The device to be examined.
 *
 * @return number of vectors.
 */
unsigned int pci_enabled_msi_vectors(struct pci_device *device)
{
	return device->msi_registers.msg32.enable ?
		1 << device->msi_registers.msg32.mme : 0;
}

static void pci_save_msi(struct pci_device *device,
			 const struct jailhouse_pci_capability *cap)
{
	u16 bdf = device->info->bdf;
	unsigned int n;

	for (n = 0; n < (device->info->msi_64bits ? 4 : 3); n++)
		device->msi_registers.raw[n] =
			pci_read_config(bdf, cap->start + n * 4, 4);
}

static void pci_restore_msi(struct pci_device *device,
			    const struct jailhouse_pci_capability *cap)
{
	unsigned int n;

	for (n = 1; n < (device->info->msi_64bits ? 4 : 3); n++)
		pci_write_config(device->info->bdf, cap->start + n * 4,
				 device->msi_registers.raw[n], 4);
}

static void pci_suppress_msix(struct pci_device *device,
			      const struct jailhouse_pci_capability *cap,
			      bool suppressed)
{
	union pci_msix_registers regs = device->msix_registers;

	if (suppressed)
		regs.fmask = 1;
	pci_write_config(device->info->bdf, cap->start, regs.raw, 4);
}

static void pci_save_msix(struct pci_device *device,
			  const struct jailhouse_pci_capability *cap)
{
	unsigned int n, r;

	device->msix_registers.raw =
		pci_read_config(device->info->bdf, cap->start, 4);

	for (n = 0; n < device->info->num_msix_vectors; n++)
		for (r = 0; r < 4; r++)
			device->msix_vectors[n].raw[r] =
				mmio_read32(&device->msix_table[n].raw[r]);
}

static void pci_restore_msix(struct pci_device *device,
			     const struct jailhouse_pci_capability *cap)
{
	unsigned int n, r;

	for (n = 0; n < device->info->num_msix_vectors; n++)
		/* only restore address/data, control is write-through */
		for (r = 0; r < 3; r++)
			mmio_write32(&device->msix_table[n].raw[r],
				     device->msix_vectors[n].raw[r]);
	pci_suppress_msix(device, cap, false);
}

/**
 * Prepare the handover of PCI devices to Jailhouse or back to Linux.
 */
void pci_prepare_handover(void)
{
	const struct jailhouse_pci_capability *cap;
	struct pci_device *device;
	unsigned int n;

	if (!root_cell.pci_devices)
		return;

	for_each_configured_pci_device(device, &root_cell) {
		if (device->cell)
			for_each_pci_cap(cap, device, n)
				if (cap->id == PCI_CAP_MSI)
					arch_pci_suppress_msi(device, cap);
				else if (cap->id == PCI_CAP_MSIX)
					pci_suppress_msix(device, cap, true);
	}
}

static int pci_add_physical_device(struct cell *cell, struct pci_device *device)
{
	unsigned int n, pages, size = device->info->msix_region_size;
	int err;

	printk("Adding PCI device %02x:%02x.%x to cell \"%s\"\n",
	       PCI_BDF_PARAMS(device->info->bdf), cell->config->name);

	for (n = 0; n < PCI_NUM_BARS; n ++)
		device->bar[n] = pci_read_config(device->info->bdf,
						 PCI_CFG_BAR + n * 4, 4);

	err = arch_pci_add_physical_device(cell, device);

	if (!err && device->info->msix_address) {
		device->msix_table = page_alloc(&remap_pool, size / PAGE_SIZE);
		if (!device->msix_table) {
			err = trace_error(-ENOMEM);
			goto error_remove_dev;
		}

		err = paging_create(&hv_paging_structs,
				    device->info->msix_address, size,
				    (unsigned long)device->msix_table,
				    PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE,
				    PAGING_NON_COHERENT);
		if (err)
			goto error_page_free;

		if (device->info->num_msix_vectors > PCI_EMBEDDED_MSIX_VECTS) {
			pages = PAGES(sizeof(union pci_msix_vector) *
				      device->info->num_msix_vectors);
			device->msix_vectors = page_alloc(&mem_pool, pages);
			if (!device->msix_vectors) {
				err = -ENOMEM;
				goto error_unmap_table;
			}
		}

		device->next_msix_device = cell->msix_device_list;
		cell->msix_device_list = device;
	}
	return err;

error_unmap_table:
	/* cannot fail, destruction of same size as construction */
	paging_destroy(&hv_paging_structs, (unsigned long)device->msix_table,
		       size, PAGING_NON_COHERENT);
error_page_free:
	page_free(&remap_pool, device->msix_table, size / PAGE_SIZE);
error_remove_dev:
	arch_pci_remove_physical_device(device);
	return err;
}

static void pci_remove_physical_device(struct pci_device *device)
{
	unsigned int size = device->info->msix_region_size;
	struct pci_device *prev_msix_device;

	printk("Removing PCI device %02x:%02x.%x from cell \"%s\"\n",
	       PCI_BDF_PARAMS(device->info->bdf), device->cell->config->name);
	arch_pci_remove_physical_device(device);
	pci_write_config(device->info->bdf, PCI_CFG_COMMAND,
			 PCI_CMD_INTX_OFF, 2);

	if (!device->msix_table)
		return;

	/* cannot fail, destruction of same size as construction */
	paging_destroy(&hv_paging_structs, (unsigned long)device->msix_table,
		       size, PAGING_NON_COHERENT);
	page_free(&remap_pool, device->msix_table, size / PAGE_SIZE);

	if (device->msix_vectors != device->msix_vector_array)
		page_free(&mem_pool, device->msix_vectors,
			  PAGES(sizeof(union pci_msix_vector) *
				device->info->num_msix_vectors));

	prev_msix_device = device->cell->msix_device_list;
	if (prev_msix_device == device) {
		device->cell->msix_device_list = device->next_msix_device;
	} else {
		while (prev_msix_device->next_msix_device != device)
			prev_msix_device = prev_msix_device->next_msix_device;
		prev_msix_device->next_msix_device = device->next_msix_device;
	}
}

/**
 * Perform PCI-specific initialization for a new cell.
 * @param cell	Cell to be initialized.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @see pci_cell_exit
 */
int pci_cell_init(struct cell *cell)
{
	unsigned int devlist_pages = PAGES(cell->config->num_pci_devices *
					   sizeof(struct pci_device));
	const struct jailhouse_pci_device *dev_infos =
		jailhouse_cell_pci_devices(cell->config);
	const struct jailhouse_pci_capability *cap;
	struct pci_device *device, *root_device;
	unsigned int ndev, ncap;
	int err;

	cell->pci_devices = page_alloc(&mem_pool, devlist_pages);
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
		device->msix_vectors = device->msix_vector_array;

		if (device->info->type == JAILHOUSE_PCI_TYPE_IVSHMEM) {
			err = pci_ivshmem_init(cell, device);
			if (err)
				goto error;

			device->cell = cell;

			continue;
		}

		root_device = pci_get_assigned_device(&root_cell,
						      dev_infos[ndev].bdf);
		if (root_device) {
			pci_remove_physical_device(root_device);
			root_device->cell = NULL;
		}

		err = pci_add_physical_device(cell, device);
		if (err)
			goto error;

		device->cell = cell;

		for_each_pci_cap(cap, device, ncap)
			if (cap->id == PCI_CAP_MSI)
				pci_save_msi(device, cap);
			else if (cap->id == PCI_CAP_MSIX)
				pci_save_msix(device, cap);
	}

	if (cell == &root_cell)
		pci_prepare_handover();

	return 0;
error:
	pci_cell_exit(cell);
	return err;
}

static void pci_return_device_to_root_cell(struct pci_device *device)
{
	struct pci_device *root_device;

	for_each_configured_pci_device(root_device, &root_cell)
		if (root_device->info->domain == device->info->domain &&
		    root_device->info->bdf == device->info->bdf) {
			if (pci_add_physical_device(&root_cell,
						    root_device) < 0)
				printk("WARNING: Failed to re-assign PCI "
				       "device to root cell\n");
			else
				root_device->cell = &root_cell;
			break;
		}
}

/**
 * Perform PCI-specific cleanup for a cell under destruction.
 * @param cell	Cell to be destructed.
 *
 * @see pci_cell_init
 */
void pci_cell_exit(struct cell *cell)
{
	unsigned int devlist_pages = PAGES(cell->config->num_pci_devices *
					   sizeof(struct pci_device));
	struct pci_device *device;

	/*
	 * Do not destroy the root cell. We will shut down the complete
	 * hypervisor instead.
	 */
	if (cell == &root_cell)
		return;

	for_each_configured_pci_device(device, cell)
		if (device->cell) {
			if (device->info->type == JAILHOUSE_PCI_TYPE_IVSHMEM) {
				pci_ivshmem_exit(device);
			} else {
				pci_remove_physical_device(device);
				pci_return_device_to_root_cell(device);
			}
		}

	page_free(&mem_pool, cell->pci_devices, devlist_pages);
}

/**
 * Apply PCI-specific configuration changes.
 * @param cell_added_removed	Cell that was added or removed to/from the
 * 				system or NULL.
 *
 * @see arch_config_commit
 */
void pci_config_commit(struct cell *cell_added_removed)
{
	const struct jailhouse_pci_capability *cap;
	struct pci_device *device;
	unsigned int n;
	int err = 0;

	if (!cell_added_removed)
		return;

	for_each_configured_pci_device(device, &root_cell)
		if (device->cell) {
			for_each_pci_cap(cap, device, n) {
				if (cap->id == PCI_CAP_MSI) {
					err = arch_pci_update_msi(device, cap);
				} else if (cap->id == PCI_CAP_MSIX) {
					err = pci_update_msix(device, cap);
					pci_suppress_msix(device, cap, false);
				}
				if (err)
					goto error;
			}
			if (device->info->type == JAILHOUSE_PCI_TYPE_IVSHMEM) {
				err = pci_ivshmem_update_msix(device);
				if (err) {
					cap = NULL;
					goto error;
				}
			}
		}
	return;

error:
	panic_printk("FATAL: Unsupported MSI/MSI-X state, device %02x:%02x.%x",
		     PCI_BDF_PARAMS(device->info->bdf));
	if (cap)
		panic_printk(", cap %d\n", cap->id);
	else
		panic_printk("\n");
	panic_stop();
}

/**
 * Shut down the PCI layer during hypervisor deactivation.
 */
void pci_shutdown(void)
{
	const struct jailhouse_pci_capability *cap;
	struct pci_device *device;
	unsigned int n;

	if (!root_cell.pci_devices)
		return;

	for_each_configured_pci_device(device, &root_cell) {
		if (!device->cell)
			continue;

		for_each_pci_cap(cap, device, n)
			if (cap->id == PCI_CAP_MSI)
				pci_restore_msi(device, cap);
			else if (cap->id == PCI_CAP_MSIX)
				pci_restore_msix(device, cap);

		if (device->cell != &root_cell)
			pci_write_config(device->info->bdf, PCI_CFG_COMMAND,
					 PCI_CMD_INTX_OFF, 2);
	}
}
