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

#define VIRTIO_VENDOR_ID	0x1af4
#define IVSHMEM_DEVICE_ID	0x1110

/* in jailhouse we can not allow dynamic remapping of the actual shared memory
 * the location and the size are stored here. A memory-BAR size of 0 will tell
 * device drivers that they are dealing with a special ivshmem device */
#define IVSHMEM_CFG_SHMEM_PTR	0x40
#define IVSHMEM_CFG_SHMEM_SZ	0x48

#define IVSHMEM_MSIX_VECTORS	1

#define IVSHMEM_REG_INTX_CTRL	0
#define IVSHMEM_REG_IVPOS	8
#define IVSHMEM_REG_DBELL	12
#define IVSHMEM_REG_LSTATE	16
#define IVSHMEM_REG_RSTATE	20

#define IVSHMEM_BAR0_SIZE	256
/*
 * Make the region two times as large as the MSI-X table to guarantee a
 * power-of-2 size (encoding constraint of a BAR).
 */
#define IVSHMEM_BAR4_SIZE	(0x10 * IVSHMEM_MSIX_VECTORS * 2)

struct ivshmem_data {
	struct ivshmem_endpoint eps[2];
	u16 bdf;
	struct ivshmem_data *next;
};

static struct ivshmem_data *ivshmem_list;

static const u32 default_cspace[IVSHMEM_CFG_SIZE / sizeof(u32)] = {
	[0x00/4] = (IVSHMEM_DEVICE_ID << 16) | VIRTIO_VENDOR_ID,
	[0x04/4] = (PCI_STS_CAPS << 16),
	[0x08/4] = PCI_DEV_CLASS_OTHER << 24,
	[0x2c/4] = (IVSHMEM_DEVICE_ID << 16) | VIRTIO_VENDOR_ID,
	[0x34/4] = IVSHMEM_CFG_MSIX_CAP,
	/* MSI-X capability */
	[IVSHMEM_CFG_MSIX_CAP/4] = (IVSHMEM_MSIX_VECTORS - 1) << 16
				   | (0x00 << 8) | PCI_CAP_MSIX,
	[(IVSHMEM_CFG_MSIX_CAP + 0x4)/4] = 4,
	[(IVSHMEM_CFG_MSIX_CAP + 0x8)/4] = 0x10 * IVSHMEM_MSIX_VECTORS | 4,
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

	if (mmio->address == IVSHMEM_REG_INTX_CTRL) {
		if (mmio->is_write) {
			ive->intx_ctrl_reg = mmio->value & IVSHMEM_INTX_ENABLE;
			arch_ivshmem_update_intx(ive);
		} else {
			mmio->value = ive->intx_ctrl_reg;
		}
		return MMIO_HANDLED;
	}

	/* read-only IVPosition */
	if (mmio->address == IVSHMEM_REG_IVPOS && !mmio->is_write) {
		mmio->value = ive->ivpos;
		return MMIO_HANDLED;
	}

	if (mmio->address == IVSHMEM_REG_DBELL) {
		if (mmio->is_write)
			ivshmem_remote_interrupt(ive);
		else
			mmio->value = 0;
		return MMIO_HANDLED;
	}

	if (mmio->address == IVSHMEM_REG_LSTATE) {
		if (mmio->is_write) {
			ive->state = mmio->value;
			ivshmem_remote_interrupt(ive);
		} else {
			mmio->value = ive->state;
		}
		return MMIO_HANDLED;
	}

	if (mmio->address == IVSHMEM_REG_RSTATE && !mmio->is_write) {
		spin_lock(&ive->remote_lock);
		mmio->value = ive->remote ? ive->remote->state : 0;
		spin_unlock(&ive->remote_lock);
		return MMIO_HANDLED;
	}

	panic_printk("FATAL: Invalid ivshmem register %s, number %02lx\n",
		     mmio->is_write ? "write" : "read", mmio->address);
	return MMIO_ERROR;
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
			mmio_region_unregister(device->cell, ive->bar0_address);
			mmio_region_unregister(device->cell, ive->bar4_address);
		}
		if (val & PCI_CMD_MEM) {
			ive->bar0_address = (*(u64 *)&device->bar[0]) & ~0xfL;
			mmio_region_register(device->cell, ive->bar0_address,
					     IVSHMEM_BAR0_SIZE,
					     ivshmem_register_mmio, ive);

			ive->bar4_address = (*(u64 *)&device->bar[4]) & ~0xfL;
			mmio_region_register(device->cell, ive->bar4_address,
					     IVSHMEM_BAR4_SIZE,
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
		mmio_region_unregister(device->cell, ive->bar0_address);
		mmio_region_unregister(device->cell, ive->bar4_address);
	}

	memset(device->bar, 0, sizeof(device->bar));
	device->msix_registers.raw = 0;

	device->bar[0] = PCI_BAR_64BIT;

	memcpy(ive->cspace, &default_cspace, sizeof(default_cspace));

	ive->cspace[0x08/4] |= device->info->shmem_protocol << 8;

	if (device->info->num_msix_vectors == 0) {
		/* let the PIN rotate based on the device number */
		ive->cspace[PCI_CFG_INT/4] =
			(((device->info->bdf >> 3) & 0x3) + 1) << 8;
		/* disable MSI-X capability */
		ive->cspace[PCI_CFG_CAPS/4] = 0;
	} else {
		device->bar[4] = PCI_BAR_64BIT;
	}

	ive->cspace[IVSHMEM_CFG_SHMEM_PTR/4] = (u32)ive->shmem->virt_start;
	ive->cspace[IVSHMEM_CFG_SHMEM_PTR/4 + 1] =
		(u32)(ive->shmem->virt_start >> 32);
	ive->cspace[IVSHMEM_CFG_SHMEM_SZ/4] = (u32)ive->shmem->size;
	ive->cspace[IVSHMEM_CFG_SHMEM_SZ/4 + 1] = (u32)(ive->shmem->size >> 32);

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
