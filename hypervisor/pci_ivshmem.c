/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Author:
 *  Henning Schild <henning.schild@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

/** @addtogroup PCI-IVSHMEM
 * Inter Cell communication using a virtual PCI device. The device provides
 * shared memory and interrupts based on MSI-X.
 *
 * The implementation in Jailhouse provides a shared memory device between
 * exactly 2 cells. The link between the two PCI devices is established by
 * choosing the same BDF, memory location, and memory size.
 */

#include <jailhouse/control.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/utils.h>
#include <jailhouse/processor.h>
#include <asm/apic.h>

#define VIRTIO_VENDOR_ID	0x1af4
#define IVSHMEM_DEVICE_ID	0x1110

/* in jailhouse we can not allow dynamic remapping of the actual shared memory
 * the location and the size are stored here. A memory-BAR size of 0 will tell
 * device drivers that they are dealing with a special ivshmem device */
#define IVSHMEM_CFG_SHMEM_PTR	0x40
#define IVSHMEM_CFG_SHMEM_SZ	0x48

#define IVSHMEM_MSIX_VECTORS	1
#define IVSHMEM_CFG_MSIX_CAP	0x50

#define IVSHMEM_REG_IVPOS	8
#define IVSHMEM_REG_DBELL	12

#define IVSHMEM_CFG_SIZE	(IVSHMEM_CFG_MSIX_CAP + 12)

struct virt_pci_bar {
	char flags;
	u64 sz;
};

struct pci_ivshmem_endpoint {
	u32 cspace[IVSHMEM_CFG_SIZE / sizeof(u32)];
	u32 ivpos;
	struct virt_pci_bar bars[3];
	struct pci_device *device;
	struct pci_ivshmem_endpoint *remote;
	struct apic_irq_message irq_msg;
};

struct pci_ivshmem_data {
	struct pci_ivshmem_endpoint eps[2];
	struct pci_ivshmem_data *next;
};

static struct pci_ivshmem_data *ivshmem_list;

static const u32 default_cspace[IVSHMEM_CFG_SIZE / sizeof(u32)] = {
	[0x00/4] = (IVSHMEM_DEVICE_ID << 16) | VIRTIO_VENDOR_ID,
	[0x04/4] = (PCI_STS_CAPS << 16),
	[0x08/4] = PCI_DEV_CLASS_MEM << 24,
	[0x2c/4] = (IVSHMEM_DEVICE_ID << 16) | VIRTIO_VENDOR_ID,
	[0x34/4] = IVSHMEM_CFG_MSIX_CAP,
	/* MSI-X capability */
	[IVSHMEM_CFG_MSIX_CAP/4] = (0xC000 + IVSHMEM_MSIX_VECTORS - 1) << 16
				   | (0x00 << 8) | PCI_CAP_MSIX,
	[(IVSHMEM_CFG_MSIX_CAP + 0x4)/4] = PCI_CFG_BAR/8 + 2,
	[(IVSHMEM_CFG_MSIX_CAP + 0x8)/4] = 0x10 * IVSHMEM_MSIX_VECTORS |
					   (PCI_CFG_BAR/8 + 2),
};

static const struct virt_pci_bar default_bars[3] = {
	{
		.flags = PCI_BAR_64BIT,
		.sz = 256,
	},
	{
		/* in jailhouse we leave this BAR empty, the shared memory
		 * location and size are in our custom registers
		 * IVSHMEM_CFG_SHMEM */
	},
	{	/* used for MSI-X vectors */
		.flags = PCI_BAR_64BIT,
		.sz = ((0x18 * IVSHMEM_MSIX_VECTORS) + 15) & ~0xf,
	}
};

static u32 ivshmem_cfg_read32(struct pci_ivshmem_endpoint *ive, u8 reg)
{
	return ive->cspace[reg / 4];
}

static u64 ivshmem_cfg_read64(struct pci_ivshmem_endpoint *ive, u8 reg)
{
	return ((u64)ivshmem_cfg_read32(ive, reg + 4) << 32) |
	       ivshmem_cfg_read32(ive, reg);
}

static u16 ivshmem_cfg_read16(struct pci_ivshmem_endpoint *ive, u8 reg)
{
	unsigned int bias = reg % 4;

	return (u16)(ivshmem_cfg_read32(ive, reg - bias) >> (bias * 8));
}

static u8 ivshmem_cfg_read8(struct pci_ivshmem_endpoint *ive, u8 reg)
{
	unsigned int bias = reg % 4;

	return (u8)(ivshmem_cfg_read32(ive, reg - bias) >> (bias * 8));
}

static bool ivshmem_is_msix_masked(struct pci_ivshmem_endpoint *ive)
{
	union pci_msix_registers c;

	/* global mask */
	c.raw = ive->cspace[IVSHMEM_CFG_MSIX_CAP/4];
	if (!c.field.enable || c.field.fmask)
		return true;

	/* local mask */
	if (ive->device->msix_vectors[0].field.ctrl & 0x1)
		return true;

	/* PCI Bus Master */
	if (!(ive->cspace[PCI_CFG_COMMAND/4] & PCI_CMD_MASTER))
		return true;

	return false;
}

static int ivshmem_update_msix(struct pci_ivshmem_endpoint *ive)
{
	union x86_msi_vector msi = {
		.raw.address = ive->device->msix_vectors[0].field.address,
		.raw.data = ive->device->msix_vectors[0].field.data,
	};
	struct apic_irq_message irq_msg;

	/* before doing anything mark the cached irq_msg as invalid,
	 * on success it will be valid on return. */
	ive->irq_msg.valid = 0;
	memory_barrier();

	if (ivshmem_is_msix_masked(ive))
		return 0;

	irq_msg = pci_translate_msi_vector(ive->device, 0, 0, msi);
	if (!irq_msg.valid)
		return 0;

	if (!apic_filter_irq_dest(ive->device->cell, &irq_msg)) {
		printk("WARNING: ivshmem MSI-X target outside of "
		       "cell \"%s\" device %02x:%02x.%x\n",
		       ive->device->cell->config->name,
		       PCI_BDF_PARAMS(ive->device->info->bdf));
		return -EPERM;
	}
	/* now copy the whole struct into our cache and mark the cache
	 * valid at the end */
	irq_msg.valid = 0;
	ive->irq_msg = irq_msg;
	memory_barrier();
	ive->irq_msg.valid = 1;

	return 0;
}

/**
 * update the command register
 * note that we only accept writes to two flags
 */
static int ivshmem_write_command(struct pci_ivshmem_endpoint *ive, u16 val)
{
	u16 *cmd = (u16 *)&ive->cspace[PCI_CFG_COMMAND/4];
	int err;

	if ((val & PCI_CMD_MASTER) != (*cmd & PCI_CMD_MASTER)) {
		*cmd = (*cmd & ~PCI_CMD_MASTER) | (val & PCI_CMD_MASTER);
		err = ivshmem_update_msix(ive);
		if (err)
			return err;
	}

	*cmd = (*cmd & ~PCI_CMD_MEM) | (val & PCI_CMD_MEM);
	return 0;
}

static void ivshmem_write_bar(struct pci_ivshmem_endpoint *ive, u8 reg, u32 val)
{
	int barn = (reg - PCI_CFG_BAR) / 8;
	struct virt_pci_bar *bar = &(ive->bars[barn]);
	u32 newval;

	if (reg & 4)
		newval = val & ((~(bar->sz - 1)) >> 32);
	else
		newval = (val & (~(bar->sz - 1) & ~0xf)) | (bar->flags & 0xf);

	ive->cspace[reg / 4] = newval;
}

static int ivshmem_msix_mmio(struct pci_ivshmem_endpoint *ive, bool is_write,
			     u32 offset, u32 *value)
{
	u32 *msix_table = (u32 *)ive->device->msix_vectors;

	if (offset % 4)
		return -1;

	/* MSI-X PBA */
	if (offset >= 0x10 * IVSHMEM_MSIX_VECTORS) {
		if (is_write) {
			return -1;
		} else {
			*value = 0;
			return 1;
		}
	/* MSI-X Table */
	} else {
		if (is_write) {
			msix_table[offset/4] = *value;
			if (ivshmem_update_msix(ive))
				return -1;
		} else {
			*value = msix_table[offset/4];
		}
		return 1;
	}
	return -1;
}

static void ivshmem_write_doorbell(struct pci_ivshmem_endpoint *ive)
{
	struct pci_ivshmem_endpoint *remote = ive->remote;
	struct apic_irq_message irq_msg;

	if (!remote)
		return;

	/* get a copy of the struct before using it, the read barrier makes
	 * sure the copy is consistent */
	irq_msg = remote->irq_msg;
	memory_load_barrier();
	if (irq_msg.valid)
		apic_send_irq(irq_msg);
}

static int ivshmem_register_mmio(struct pci_ivshmem_endpoint *ive,
				 bool is_write, u32 offset, u32 *value)
{
	/* IVPosition, ro and always returns 0 */
	if (offset == IVSHMEM_REG_IVPOS && !is_write) {
		*value = ive->ivpos;
		return 1;
	}

	if (offset == IVSHMEM_REG_DBELL) {
		if (is_write) {
			ivshmem_write_doorbell(ive);
		} else {
			*value = 0;
		}
		return 1;
	}
	return -1;
}

static int ivshmem_write_msix_control(struct pci_ivshmem_endpoint *ive, u32 val)
{
	union pci_msix_registers *p = (union pci_msix_registers *)&val;
	union pci_msix_registers newval = {
		.raw = ive->cspace[IVSHMEM_CFG_MSIX_CAP/4]
	};

	newval.field.enable = p->field.enable;
	newval.field.fmask = p->field.fmask;
	if (ive->cspace[IVSHMEM_CFG_MSIX_CAP/4] != newval.raw) {
		ive->cspace[IVSHMEM_CFG_MSIX_CAP/4] = newval.raw;
		return ivshmem_update_msix(ive);
	}
	return 0;
}

static enum pci_access ivshmem_cfg_write32(struct pci_ivshmem_endpoint *ive,
					   u8 reg, u32 val)
{
	switch (reg) {
	case PCI_CFG_COMMAND:
		if(ivshmem_write_command(ive, val & 0xffff))
			return PCI_ACCESS_REJECT;
		break;
	case PCI_CFG_BAR ... (PCI_CFG_BAR + 3*8):
		ivshmem_write_bar(ive, reg, val);
		break;
	case IVSHMEM_CFG_MSIX_CAP:
		if (ivshmem_write_msix_control(ive, val))
			return PCI_ACCESS_REJECT;
	}
	return PCI_ACCESS_DONE;
}

static enum pci_access ivshmem_cfg_write16(struct pci_ivshmem_endpoint *ive,
					   u8 reg, u16 val)
{
	u32 row, shift;

	shift = (reg % 4) * 8;
	row = ive->cspace[reg / 4];
	row &= ~(BYTE_MASK(2) << shift);
	row |= val << shift;

	return ivshmem_cfg_write32(ive, reg - (reg % 4), row);
}

static enum pci_access ivshmem_cfg_write8(struct pci_ivshmem_endpoint *ive,
					  u8 reg, u8 val)
{
	u32 row;
	u8 *rowp;

	row = ive->cspace[reg / 4];
	rowp = (u8 *)&row;
	rowp[(reg % 4)] = val;

	return ivshmem_cfg_write32(ive, reg - (reg % 4), row);
}


static struct pci_ivshmem_data **ivshmem_find(struct pci_device *d,
					      int *cellnum)
{
	struct pci_ivshmem_data **ivp, *iv;
	u16 bdf2;

	for (ivp = &ivshmem_list; *ivp; ivp = &((*ivp)->next)) {
		iv = *ivp;
		bdf2 = iv->eps[0].device->info->bdf;
		if (d->info->bdf == bdf2) {
			if (iv->eps[0].device == d) {
				if (cellnum)
					*cellnum = 0;
				return ivp;
			}
			if (iv->eps[1].device == d) {
				if (cellnum)
					*cellnum = 1;
				return ivp;
			}
			if (!cellnum)
				return ivp;
		}
	}

	return NULL;
}

static void ivshmem_connect_cell(struct pci_ivshmem_data *iv,
				 struct pci_device *d,
				 const struct jailhouse_memory *mem,
				 int cellnum)
{
	struct pci_ivshmem_endpoint *remote = &iv->eps[(cellnum + 1) % 2];
	struct pci_ivshmem_endpoint *ive = &iv->eps[cellnum];

	memcpy(ive->cspace, &default_cspace, sizeof(default_cspace));
	memcpy(ive->bars, &default_bars, sizeof(default_bars));

	ive->cspace[IVSHMEM_CFG_SHMEM_PTR/4] = (u32)mem->virt_start;
	ive->cspace[IVSHMEM_CFG_SHMEM_PTR/4 + 1] = (u32)(mem->virt_start >> 32);
	ive->cspace[IVSHMEM_CFG_SHMEM_SZ/4] = (u32)mem->size;
	ive->cspace[IVSHMEM_CFG_SHMEM_SZ/4 + 1] = (u32)(mem->size >> 32);

	ive->device = d;
	if (remote->device) {
		ive->remote = remote;
		remote->remote = ive;
		ive->ivpos = (remote->ivpos + 1) % 2;
	} else {
		ive->ivpos = cellnum;
		ive->remote = NULL;
		remote->remote = NULL;
	}
	d->ivshmem_endpoint = ive;
}

static void ivshmem_disconnect_cell(struct pci_ivshmem_data *iv, int cellnum)
{
	struct pci_ivshmem_endpoint *remote = &iv->eps[(cellnum + 1) % 2];
	struct pci_ivshmem_endpoint *ive = &iv->eps[cellnum];

	ive->device->ivshmem_endpoint = NULL;
	ive->device = NULL;
	ive->remote = NULL;
	remote->remote = NULL;
}

/**
 * Handler for MMIO-accesses to this virtual PCI devices memory. Both for the
 * BAR containing the registers, and the MSI-X BAR.
 * @param cell		The cell that issued the access.
 * @param is_write	True if write access.
 * @param addr		Address accessed.
 * @param value		Pointer to value for reading/writing.
 *
 * @return 1 if handled successfully, 0 if unhandled, -1 on access error.
 *
 * @see pci_mmio_access_handler
 */
int ivshmem_mmio_access_handler(const struct cell *cell, bool is_write,
				u64 addr, u32 *value)
{
	struct pci_ivshmem_endpoint *ive;
	struct pci_device *device;
	u64 mem_start, mem_sz;

	for (device = cell->virtual_device_list; device;
	     device = device->next_virtual_device) {
		ive = device->ivshmem_endpoint;
		if (!ive)
			continue;
		if ((ive->cspace[PCI_CFG_COMMAND/4] & PCI_CMD_MEM) == 0)
			continue;

		/* register BAR access */
		mem_start = ivshmem_cfg_read64(ive, PCI_CFG_BAR) & ~0xf;
		mem_sz = ive->bars[0].sz;
		if (addr >= mem_start && addr <= (mem_start + mem_sz - 4))
			return ivshmem_register_mmio(ive, is_write,
						     addr - mem_start,
						     value);

		/* MSI-X BAR access */
		mem_start = ivshmem_cfg_read64(ive, PCI_CFG_BAR + 2 * 8) & ~0xf;
		mem_sz = ive->bars[2].sz;
		if (addr >= mem_start && addr <= (mem_start + mem_sz - 4))
			return ivshmem_msix_mmio(ive, is_write,
						 addr - mem_start, value);
	}

	return 0;
}

/**
 * Handler for MMIO-write-accesses to PCI config space of this virtual device.
 * @param dev		The device that access should be performed on.
 * @param address	Config space address accessed.
 * @param sz		The amount of bytes to write.
 * @param value		The value to write to the config space.
 *
 * @return PCI_ACCESS_REJECT or PCI_ACCESS_DONE.
 *
 * @see pci_cfg_write_moderate
 */
enum pci_access pci_ivshmem_cfg_write(struct pci_device *dev, u16 address,
				      u8 sz, u32 value)
{
	struct pci_ivshmem_endpoint *ive = dev->ivshmem_endpoint;

	if (address > (sizeof(default_cspace) - sz))
		return PCI_ACCESS_REJECT;

	if (!ive)
		return PCI_ACCESS_REJECT;

	switch (sz) {
	case 1:
		return ivshmem_cfg_write8(ive, address, (u8)value);
	case 2:
		return ivshmem_cfg_write16(ive, address, (u16)value);
	case 4:
		return ivshmem_cfg_write32(ive, address, value);
	default:
		return PCI_ACCESS_REJECT;
	}
}

/**
 * Handler for MMIO-read-accesses to PCI config space of this virtual device.
 * @param dev		The device that access should be performed on.
 * @param address	Config space address accessed.
 * @param sz		The amount of bytes to read.
 * @param value		Pointer to the return value.
 *
 * @return PCI_ACCESS_DONE.
 *
 * @see pci_cfg_read_moderate
 */
enum pci_access pci_ivshmem_cfg_read(struct pci_device *dev, u16 address,
				     u8 sz, u32 *value)
{
	struct pci_ivshmem_endpoint *ive = dev->ivshmem_endpoint;

	if (address > (sizeof(default_cspace) - sz))
		goto fail;
	if (!ive)
		goto fail;

	switch (sz) {
	case 1:
		*value = (u32)ivshmem_cfg_read8(ive, address);
		break;
	case 2:
		*value = (u32)ivshmem_cfg_read16(ive, address);
		break;
	case 4:
		*value = ivshmem_cfg_read32(ive, address);
		break;
	default:
		goto fail;
	}
	return PCI_ACCESS_DONE;

fail:
	*value = -1;
	/* the caller can not deal with PCI_ACCESS_REJECT for reads */
	return PCI_ACCESS_DONE;
}

/**
 * Update cached MSI-X state of the given ivshmem device.
 * @param dev	The device to be updated.
 *
 * @return 0 on success, negative error code otherwise.
 */
int pci_ivshmem_update_msix(struct pci_device *dev)
{
	return ivshmem_update_msix(dev->ivshmem_endpoint);
}

/**
 * Register a new ivshmem device.
 * @param cell		The cell the device should be attached to.
 * @param dev		The device to be registered.
 *
 * @return 0 on success, negative error code otherwise.
 */
int pci_ivshmem_init(struct cell *cell, struct pci_device *dev)
{
	const struct jailhouse_memory *mem, *mem0;
	struct pci_ivshmem_data **ivp;
	struct pci_device *dev0;

	if (dev->info->num_msix_vectors != 1)
		return trace_error(-EINVAL);

	if (dev->info->shmem_region >= cell->config->num_memory_regions)
		return trace_error(-EINVAL);

	mem = jailhouse_cell_mem_regions(cell->config)
		+ dev->info->shmem_region;
	ivp = ivshmem_find(dev, NULL);
	if (ivp) {
		dev0 = (*ivp)->eps[0].device;
		mem0 = jailhouse_cell_mem_regions(dev0->cell->config) +
			dev0->info->shmem_region;

		/* we already have a datastructure, connect second endpoint */
		if ((mem0->phys_start == mem->phys_start) &&
		    (mem0->size == mem->size)) {
			if ((*ivp)->eps[1].device)
				return trace_error(-EBUSY);
			ivshmem_connect_cell(*ivp, dev, mem, 1);
			printk("Virtual PCI connection established "
				"\"%s\" <--> \"%s\"\n",
				cell->config->name, dev0->cell->config->name);
			goto connected;
		}
	}

	/* this is the first endpoint, allocate a new datastructure */
	for (ivp = &ivshmem_list; *ivp; ivp = &((*ivp)->next))
		; /* empty loop */
	*ivp = page_alloc(&mem_pool, 1);
	if (!(*ivp))
		return -ENOMEM;
	ivshmem_connect_cell(*ivp, dev, mem, 0);

connected:
	dev->cell = cell;
	printk("Adding virtual PCI device %02x:%02x.%x to cell \"%s\"\n",
	       PCI_BDF_PARAMS(dev->info->bdf), cell->config->name);

	return 0;
}

/**
 * Unregister a ivshmem device, typically when the corresponding cell exits.
 * @param dev		The device to be stopped.
 *
 */
void pci_ivshmem_exit(struct pci_device *dev)
{
	struct pci_ivshmem_data **ivp, *iv;
	int cellnum;

	ivp = ivshmem_find(dev, &cellnum);
	if (!ivp || !(*ivp))
		return;

	iv = *ivp;

	if (iv->eps[0].device == dev) {
		if (!iv->eps[1].device) {
			*ivp = iv->next;
			page_free(&mem_pool, iv, 1);
			return;
		}
		iv->eps[0] = iv->eps[1];
	}
	ivshmem_disconnect_cell(iv, 1);
}
