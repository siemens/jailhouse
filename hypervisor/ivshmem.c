/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2019
 *
 * Authors:
 *  Henning Schild <henning.schild@siemens.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

/** @addtogroup IVSHMEM
 * Inter Cell communication using a virtual PCI device. The device provides
 * shared memory and interrupts based on MSI-X.
 *
 * The implementation in Jailhouse provides a shared memory device between
 * exactly 2 cells. The link between the two PCI devices is established by
 * choosing the same BDF, memory location, and memory size.
 */

#include <jailhouse/ivshmem.h>
#include <jailhouse/mmio.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/utils.h>
#include <jailhouse/processor.h>
#include <jailhouse/percpu.h>

#define PCI_VENDOR_ID_SIEMENS		0x110a
#define IVSHMEM_DEVICE_ID		0x4106

#define IVSHMEM_CFG_VNDR_CAP		0x40
#define IVSHMEM_CFG_MSIX_CAP		(IVSHMEM_CFG_VNDR_CAP + \
					 IVSHMEM_CFG_VNDR_LEN)

#define IVSHMEM_CFG_SHMEM_RW_SZ		(IVSHMEM_CFG_VNDR_CAP + 0x08)
#define IVSHMEM_CFG_SHMEM_ADDR		(IVSHMEM_CFG_VNDR_CAP + 0x18)
#define IVSHMEM_CFG_VNDR_LEN		0x20

#define IVSHMEM_MSIX_VECTORS	1

/*
 * Make the region two times as large as the MSI-X table to guarantee a
 * power-of-2 size (encoding constraint of a BAR).
 */
#define IVSHMEM_MSIX_SIZE		(0x10 * IVSHMEM_MSIX_VECTORS * 2)

#define IVSHMEM_REG_IVPOS		0x00
#define IVSHMEM_REG_INTX_CTRL		0x08
#define IVSHMEM_REG_DOORBELL		0x0c
#define IVSHMEM_REG_LSTATE		0x10
#define IVSHMEM_REG_RSTATE		0x14

struct ivshmem_data {
	struct ivshmem_endpoint eps[2];
	u16 bdf;
	struct ivshmem_data *next;
};

static struct ivshmem_data *ivshmem_list;

static const u32 default_cspace[IVSHMEM_CFG_SIZE / sizeof(u32)] = {
	[0x00/4] = (IVSHMEM_DEVICE_ID << 16) | PCI_VENDOR_ID_SIEMENS,
	[0x04/4] = (PCI_STS_CAPS << 16),
	[0x08/4] = PCI_DEV_CLASS_OTHER << 24,
	[0x2c/4] = (IVSHMEM_DEVICE_ID << 16) | PCI_VENDOR_ID_SIEMENS,
	[PCI_CFG_CAPS/4] = IVSHMEM_CFG_VNDR_CAP,
	[IVSHMEM_CFG_VNDR_CAP/4] = (IVSHMEM_CFG_VNDR_LEN << 16) |
				(IVSHMEM_CFG_MSIX_CAP << 8) | PCI_CAP_ID_VNDR,
	[IVSHMEM_CFG_MSIX_CAP/4] = (IVSHMEM_MSIX_VECTORS - 1) << 16 |
				   (0x00 << 8) | PCI_CAP_ID_MSIX,
	[(IVSHMEM_CFG_MSIX_CAP + 0x4)/4] = 1,
	[(IVSHMEM_CFG_MSIX_CAP + 0x8)/4] = 0x10 * IVSHMEM_MSIX_VECTORS | 1,
};

static void ivshmem_remote_interrupt(struct ivshmem_endpoint *ive)
{
	/*
	 * Hold the remote lock while sending the interrupt so that
	 * ivshmem_exit can synchronize on the completion of the delivery.
	 */
	spin_lock(&ive->remote_lock);
	if (ive->remote)
		arch_ivshmem_trigger_interrupt(ive->remote);
	spin_unlock(&ive->remote_lock);
}

static enum mmio_result ivshmem_register_mmio(void *arg,
					      struct mmio_access *mmio)
{
	struct ivshmem_endpoint *ive = arg;

	switch (mmio->address) {
	case IVSHMEM_REG_INTX_CTRL:
		if (mmio->is_write) {
			ive->intx_ctrl_reg = mmio->value & IVSHMEM_INTX_ENABLE;
			arch_ivshmem_update_intx(ive);
		} else {
			mmio->value = ive->intx_ctrl_reg;
		}
		break;
	case IVSHMEM_REG_IVPOS:
		/* read-only IVPosition */
		mmio->value = ive->ivpos;
		break;
	case IVSHMEM_REG_DOORBELL:
		if (mmio->is_write)
			ivshmem_remote_interrupt(ive);
		else
			mmio->value = 0;
		break;
	case IVSHMEM_REG_LSTATE:
		if (mmio->is_write) {
			ive->state = mmio->value;
			ivshmem_remote_interrupt(ive);
		} else {
			mmio->value = ive->state;
		}
		break;
	case IVSHMEM_REG_RSTATE:
		/* read-only remote state */
		spin_lock(&ive->remote_lock);
		mmio->value = ive->remote ? ive->remote->state : 0;
		spin_unlock(&ive->remote_lock);
		break;
	default:
		/* ignore any other access */
		mmio->value = 0;
		break;
	}
	return MMIO_HANDLED;
}

/**
 * Check if MSI-X doorbell interrupt is masked.
 * @param ive		Ivshmem endpoint the mask should be checked for.
 *
 * @return True if MSI-X interrupt is masked.
 */
bool ivshmem_is_msix_masked(struct ivshmem_endpoint *ive)
{
	union pci_msix_registers c;

	/* global mask */
	c.raw = ive->cspace[IVSHMEM_CFG_MSIX_CAP/4];
	if (!c.enable || c.fmask)
		return true;

	/* local mask */
	if (ive->device->msix_vectors[0].masked)
		return true;

	/* PCI Bus Master */
	if (!(ive->cspace[PCI_CFG_COMMAND/4] & PCI_CMD_MASTER))
		return true;

	return false;
}

static enum mmio_result ivshmem_msix_mmio(void *arg, struct mmio_access *mmio)
{
	struct ivshmem_endpoint *ive = arg;
	u32 *msix_table = (u32 *)ive->device->msix_vectors;

	if (mmio->address % 4)
		goto fail;

	/* MSI-X PBA */
	if (mmio->address >= 0x10 * IVSHMEM_MSIX_VECTORS) {
		if (mmio->is_write) {
			goto fail;
		} else {
			mmio->value = 0;
			return MMIO_HANDLED;
		}
	/* MSI-X Table */
	} else {
		if (mmio->is_write) {
			msix_table[mmio->address / 4] = mmio->value;
			if (arch_ivshmem_update_msix(ive->device))
				return MMIO_ERROR;
		} else {
			mmio->value = msix_table[mmio->address / 4];
		}
		return MMIO_HANDLED;
	}

fail:
	panic_printk("FATAL: Invalid PCI MSI-X table/PBA access, device "
		     "%02x:%02x.%x\n", PCI_BDF_PARAMS(ive->device->info->bdf));
	return MMIO_ERROR;
}

/**
 * update the command register
 * note that we only accept writes to two flags
 */
static int ivshmem_write_command(struct ivshmem_endpoint *ive, u16 val)
{
	u16 *cmd = (u16 *)&ive->cspace[PCI_CFG_COMMAND/4];
	struct pci_device *device = ive->device;
	int err;

	if ((val & PCI_CMD_MASTER) != (*cmd & PCI_CMD_MASTER)) {
		*cmd = (*cmd & ~PCI_CMD_MASTER) | (val & PCI_CMD_MASTER);
		err = arch_ivshmem_update_msix(device);
		if (err)
			return err;
	}

	if ((val & PCI_CMD_MEM) != (*cmd & PCI_CMD_MEM)) {
		if (*cmd & PCI_CMD_MEM) {
			mmio_region_unregister(device->cell, ive->ioregion[0]);
			mmio_region_unregister(device->cell, ive->ioregion[1]);
		}
		if (val & PCI_CMD_MEM) {
			ive->ioregion[0] = device->bar[0] & ~0xf;
			/* Derive the size of region 0 from its BAR mask. */
			mmio_region_register(device->cell, ive->ioregion[0],
					     ~device->info->bar_mask[0] + 1,
					     ivshmem_register_mmio, ive);

			ive->ioregion[1] = device->bar[1] & ~0xf;
			mmio_region_register(device->cell, ive->ioregion[1],
					     IVSHMEM_MSIX_SIZE,
					     ivshmem_msix_mmio, ive);
		}
		*cmd = (*cmd & ~PCI_CMD_MEM) | (val & PCI_CMD_MEM);
	}

	return 0;
}

static int ivshmem_write_msix_control(struct ivshmem_endpoint *ive, u32 val)
{
	union pci_msix_registers *p = (union pci_msix_registers *)&val;
	union pci_msix_registers newval = {
		.raw = ive->cspace[IVSHMEM_CFG_MSIX_CAP/4]
	};

	newval.enable = p->enable;
	newval.fmask = p->fmask;
	if (ive->cspace[IVSHMEM_CFG_MSIX_CAP/4] != newval.raw) {
		ive->cspace[IVSHMEM_CFG_MSIX_CAP/4] = newval.raw;
		return arch_ivshmem_update_msix(ive->device);
	}
	return 0;
}

/**
 * Handler for MMIO-write-accesses to PCI config space of this virtual device.
 * @param device	The device that access should be performed on.
 * @param row		Config space DWORD row of the access.
 * @param mask		Mask selected the DWORD bytes to write.
 * @param value		DWORD to write to the config space.
 *
 * @return PCI_ACCESS_REJECT or PCI_ACCESS_DONE.
 *
 * @see pci_cfg_write_moderate
 */
enum pci_access ivshmem_pci_cfg_write(struct pci_device *device,
				      unsigned int row, u32 mask, u32 value)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;

	if (row >= ARRAY_SIZE(default_cspace))
		return PCI_ACCESS_REJECT;

	value |= ive->cspace[row] & ~mask;

	switch (row) {
	case PCI_CFG_COMMAND / 4:
		if (ivshmem_write_command(ive, value))
			return PCI_ACCESS_REJECT;
		break;
	case IVSHMEM_CFG_MSIX_CAP / 4:
		if (ivshmem_write_msix_control(ive, value))
			return PCI_ACCESS_REJECT;
	}
	return PCI_ACCESS_DONE;
}

/**
 * Handler for MMIO-read-accesses to PCI config space of this virtual device.
 * @param device	The device that access should be performed on.
 * @param address	Config space address accessed.
 * @param value		Pointer to the return value.
 *
 * @return PCI_ACCESS_DONE.
 *
 * @see pci_cfg_read_moderate
 */
enum pci_access ivshmem_pci_cfg_read(struct pci_device *device, u16 address,
				     u32 *value)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;

	if (address < sizeof(default_cspace))
		*value = ive->cspace[address / 4] >> ((address % 4) * 8);
	else
		*value = -1;
	return PCI_ACCESS_DONE;
}

/**
 * Register a new ivshmem device.
 * @param cell		The cell the device should be attached to.
 * @param device	The device to be registered.
 *
 * @return 0 on success, negative error code otherwise.
 */
int ivshmem_init(struct cell *cell, struct pci_device *device)
{
	const struct jailhouse_pci_device *dev_info = device->info;
	const struct jailhouse_memory *mem, *peer_mem;
	struct ivshmem_endpoint *ive, *remote;
	struct pci_device *peer_dev;
	struct ivshmem_data *iv;
	unsigned int id = 0;

	printk("Adding virtual PCI device %02x:%02x.%x to cell \"%s\"\n",
	       PCI_BDF_PARAMS(dev_info->bdf), cell->config->name);

	if (dev_info->shmem_region >= cell->config->num_memory_regions)
		return trace_error(-EINVAL);

	mem = jailhouse_cell_mem_regions(cell->config) + dev_info->shmem_region;

	for (iv = ivshmem_list; iv; iv = iv->next)
		if (iv->bdf == dev_info->bdf)
			break;

	if (iv) {
		id = iv->eps[0].device ? 1 : 0;
		if (iv->eps[id].device)
			return trace_error(-EBUSY);

		peer_dev = iv->eps[id ^ 1].device;
		peer_mem = jailhouse_cell_mem_regions(peer_dev->cell->config) +
			peer_dev->info->shmem_region;

		/* check that the regions and protocols of both peers match */
		if (peer_mem->phys_start != mem->phys_start ||
		    peer_mem->size != mem->size ||
		    peer_dev->info->shmem_protocol != dev_info->shmem_protocol)
			return trace_error(-EINVAL);

		printk("Shared memory connection established: "
		       "\"%s\" <--> \"%s\"\n",
		       cell->config->name, peer_dev->cell->config->name);
	} else {
		iv = page_alloc(&mem_pool, 1);
		if (!iv)
			return -ENOMEM;

		iv->bdf = dev_info->bdf;
		iv->next = ivshmem_list;
		ivshmem_list = iv;
	}

	ive = &iv->eps[id];
	remote = &iv->eps[id ^ 1];

	ive->device = device;
	ive->shmem = mem;
	ive->ivpos = id;
	device->ivshmem_endpoint = ive;
	if (remote->device) {
		ive->remote = remote;
		remote->remote = ive;
	}

	device->cell = cell;
	pci_reset_device(device);

	return 0;
}

void ivshmem_reset(struct pci_device *device)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;

	if (ive->cspace[PCI_CFG_COMMAND/4] & PCI_CMD_MEM) {
		mmio_region_unregister(device->cell, ive->ioregion[0]);
		mmio_region_unregister(device->cell, ive->ioregion[1]);
	}

	memset(device->bar, 0, sizeof(device->bar));
	device->msix_registers.raw = 0;

	memcpy(ive->cspace, &default_cspace, sizeof(default_cspace));

	ive->cspace[0x08/4] |= device->info->shmem_protocol << 8;

	if (device->info->num_msix_vectors == 0) {
		/* let the PIN rotate based on the device number */
		ive->cspace[PCI_CFG_INT/4] =
			(((device->info->bdf >> 3) & 0x3) + 1) << 8;
		/* disable MSI-X capability */
		ive->cspace[IVSHMEM_CFG_VNDR_CAP/4] &= 0xffff00ff;
	}

	ive->cspace[IVSHMEM_CFG_SHMEM_RW_SZ/4] = (u32)ive->shmem->size;
	ive->cspace[IVSHMEM_CFG_SHMEM_RW_SZ/4 + 1] =
		(u32)(ive->shmem->size >> 32);
	ive->cspace[IVSHMEM_CFG_SHMEM_ADDR/4] = (u32)ive->shmem->virt_start;
	ive->cspace[IVSHMEM_CFG_SHMEM_ADDR/4 + 1] =
		(u32)(ive->shmem->virt_start >> 32);

	ive->state = 0;
}

/**
 * Unregister a ivshmem device, typically when the corresponding cell exits.
 * @param device	The device to be stopped.
 *
 */
void ivshmem_exit(struct pci_device *device)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;
	struct ivshmem_endpoint *remote = ive->remote;
	struct ivshmem_data **ivp, *iv;

	if (remote) {
		/*
		 * The spinlock synchronizes the disconnection of the remote
		 * device with any in-flight interrupts targeting the device
		 * to be destroyed.
		 */
		spin_lock(&remote->remote_lock);
		remote->remote = NULL;
		spin_unlock(&remote->remote_lock);

		ivshmem_remote_interrupt(ive);

		ive->device = NULL;
	} else {
		for (ivp = &ivshmem_list; *ivp; ivp = &(*ivp)->next) {
			iv = *ivp;
			if (&iv->eps[ive->ivpos] == ive) {
				*ivp = iv->next;
				page_free(&mem_pool, iv, 1);
				break;
			}
		}
	}
}
