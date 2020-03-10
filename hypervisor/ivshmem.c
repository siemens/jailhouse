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
 * 2 or more cells. The link between the PCI devices is established by
 * choosing the same BDF.
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

#define IVSHMEM_MAX_PEERS		12

#define IVSHMEM_CFG_VNDR_CAP		0x40
#define IVSHMEM_CFG_MSIX_CAP		(IVSHMEM_CFG_VNDR_CAP + \
					 IVSHMEM_CFG_VNDR_LEN)

#define IVSHMEM_CFG_SHMEM_STATE_TAB_SZ	(IVSHMEM_CFG_VNDR_CAP + 0x04)
#define IVSHMEM_CFG_SHMEM_RW_SZ		(IVSHMEM_CFG_VNDR_CAP + 0x08)
#define IVSHMEM_CFG_SHMEM_OUTPUT_SZ	(IVSHMEM_CFG_VNDR_CAP + 0x10)
#define IVSHMEM_CFG_SHMEM_ADDR		(IVSHMEM_CFG_VNDR_CAP + 0x18)
#define IVSHMEM_CFG_VNDR_LEN		0x20

#define IVSHMEM_CFG_ONESHOT_INT		(1 << 24)

/*
 * Make the region two times as large as the MSI-X table to guarantee a
 * power-of-2 size (encoding constraint of a BAR).
 */
#define IVSHMEM_MSIX_SIZE		(0x10 * IVSHMEM_MSIX_VECTORS * 2)

#define IVSHMEM_REG_ID			0x00
#define IVSHMEM_REG_MAX_PEERS		0x04
#define IVSHMEM_REG_INT_CTRL		0x08
#define IVSHMEM_REG_DOORBELL		0x0c
#define IVSHMEM_REG_STATE		0x10

struct ivshmem_link {
	struct ivshmem_endpoint eps[IVSHMEM_MAX_PEERS];
	unsigned int peers;
	u16 bdf;
	struct ivshmem_link *next;
};

static struct ivshmem_link *ivshmem_links;

static const u32 default_cspace[IVSHMEM_CFG_SIZE / sizeof(u32)] = {
	[0x00/4] = (IVSHMEM_DEVICE_ID << 16) | PCI_VENDOR_ID_SIEMENS,
	[0x04/4] = (PCI_STS_CAPS << 16),
	[0x08/4] = PCI_DEV_CLASS_OTHER << 24,
	[0x2c/4] = (IVSHMEM_DEVICE_ID << 16) | PCI_VENDOR_ID_SIEMENS,
	[PCI_CFG_CAPS/4] = IVSHMEM_CFG_VNDR_CAP,
	[IVSHMEM_CFG_VNDR_CAP/4] = (IVSHMEM_CFG_VNDR_LEN << 16) |
				(IVSHMEM_CFG_MSIX_CAP << 8) | PCI_CAP_ID_VNDR,
	[IVSHMEM_CFG_MSIX_CAP/4] = (0x00 << 8) | PCI_CAP_ID_MSIX,
	[(IVSHMEM_CFG_MSIX_CAP + 0x4)/4] = 1,
	[(IVSHMEM_CFG_MSIX_CAP + 0x8)/4] = 0x10 * IVSHMEM_MSIX_VECTORS | 1,
};

static void ivshmem_trigger_interrupt(struct ivshmem_endpoint *ive,
				      unsigned int vector)
{
	/*
	 * Hold the IRQ lock while sending the interrupt so that ivshmem_exit
	 * and ivshmem_register_mmio can synchronize on the completion of the
	 * delivery.
	 */
	spin_lock(&ive->irq_lock);

	if (ive->int_ctrl_reg & IVSHMEM_INT_ENABLE) {
		if (ive->cspace[IVSHMEM_CFG_VNDR_CAP/4] &
		    IVSHMEM_CFG_ONESHOT_INT)
			ive->int_ctrl_reg = 0;

		arch_ivshmem_trigger_interrupt(ive, vector);
	}

	spin_unlock(&ive->irq_lock);
}

static u32 *ivshmem_map_state_table(struct ivshmem_endpoint *ive)
{
	/*
	 * Cannot fail: upper levels of page table were already created by
	 * paging_init, and we always map single pages, thus only update the
	 * leaf entry and do not have to deal with huge pages.
	 */
	paging_create(&this_cpu_data()->pg_structs,
		      ive->shmem[0].phys_start, PAGE_SIZE,
		      TEMPORARY_MAPPING_BASE, PAGE_DEFAULT_FLAGS,
		      PAGING_NON_COHERENT | PAGING_NO_HUGE);

	return (u32 *)TEMPORARY_MAPPING_BASE;
}


static void ivshmem_write_state(struct ivshmem_endpoint *ive, u32 new_state)
{
	const struct jailhouse_pci_device *dev_info = ive->device->info;
	u32 *state_table = ivshmem_map_state_table(ive);
	struct ivshmem_endpoint *target_ive;
	unsigned int id;

	state_table[dev_info->shmem_dev_id] = new_state;
	memory_barrier();

	if (ive->state != new_state) {
		ive->state = new_state;

		for (id = 0; id < dev_info->shmem_peers; id++) {
			target_ive = &ive->link->eps[id];
			if (target_ive != ive)
				ivshmem_trigger_interrupt(target_ive, 0);
		}
	}
}

int ivshmem_update_msix_vector(struct pci_device *device, unsigned int vector)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;
	union pci_msix_registers cap;
	bool enabled;

	cap.raw = ive->cspace[IVSHMEM_CFG_MSIX_CAP/4];
	enabled = cap.enable && !cap.fmask &&
		!device->msix_vectors[vector].masked &&
		ive->cspace[PCI_CFG_COMMAND/4] & PCI_CMD_MASTER;

	return arch_ivshmem_update_msix(ive, vector, enabled);
}

int ivshmem_update_msix(struct pci_device *device)
{
	unsigned int vector, num_vectors = device->info->num_msix_vectors;
	int err = 0;

	for (vector = 0; vector < num_vectors && !err; vector++)
		err = ivshmem_update_msix_vector(device, vector);

	return err;
}

static void ivshmem_update_intx(struct ivshmem_endpoint *ive)
{
	bool masked = ive->cspace[PCI_CFG_COMMAND/4] & PCI_CMD_INTX_OFF;

	if (ive->device->info->num_msix_vectors == 0)
		arch_ivshmem_update_intx(ive, !masked);
}

static enum mmio_result ivshmem_register_mmio(void *arg,
					      struct mmio_access *mmio)
{
	struct ivshmem_endpoint *target_ive, *ive = arg;
	unsigned int num_vectors, vector, target;

	switch (mmio->address) {
	case IVSHMEM_REG_ID:
		/* read-only ID */
		mmio->value = ive->device->info->shmem_dev_id;
		break;
	case IVSHMEM_REG_MAX_PEERS:
		/* read-only number of peers */
		mmio->value = ive->device->info->shmem_peers;
		break;
	case IVSHMEM_REG_INT_CTRL:
		if (mmio->is_write) {
			/*
			 * The spinlock acts as barrier, ensuring that
			 * interrupts are disabled on return.
			 */
			spin_lock(&ive->irq_lock);
			ive->int_ctrl_reg = mmio->value & IVSHMEM_INT_ENABLE;
			spin_unlock(&ive->irq_lock);

			ivshmem_update_intx(ive);
			if (ivshmem_update_msix(ive->device))
				return MMIO_ERROR;
		} else {
			mmio->value = ive->int_ctrl_reg;
		}
		break;
	case IVSHMEM_REG_DOORBELL:
		if (mmio->is_write) {
			/*
			 * All peers have the same number of MSI-X vectors,
			 * thus we can derive the limit from the local device.
			 */
			num_vectors = ive->device->info->num_msix_vectors;
			if (num_vectors == 0)
				num_vectors = 1; /* INTx means one vector */

			vector = GET_FIELD(mmio->value, 15, 0);
			/* ignore out-of-range requests */
			if (vector >= num_vectors)
				break;

			target = GET_FIELD(mmio->value, 31, 16);
			if (target >= IVSHMEM_MAX_PEERS)
				break;

			target_ive = &ive->link->eps[target];

			ivshmem_trigger_interrupt(target_ive, vector);
		} else {
			mmio->value = 0;
		}
		break;
	case IVSHMEM_REG_STATE:
		if (mmio->is_write)
			ivshmem_write_state(ive, mmio->value);
		else
			mmio->value = ive->state;
		break;
	default:
		/* ignore any other access */
		mmio->value = 0;
		break;
	}
	return MMIO_HANDLED;
}

static enum mmio_result ivshmem_msix_mmio(void *arg, struct mmio_access *mmio)
{
	unsigned int vector = mmio->address / sizeof(union pci_msix_vector);
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
		if (vector >= ive->device->info->num_msix_vectors)
			goto fail;
		if (mmio->is_write) {
			msix_table[mmio->address / 4] = mmio->value;
			if (ivshmem_update_msix_vector(ive->device, vector))
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
		err = ivshmem_update_msix(device);
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

	if ((val & PCI_CMD_INTX_OFF) != (*cmd & PCI_CMD_INTX_OFF)) {
		*cmd = (*cmd & ~PCI_CMD_INTX_OFF) | (val & PCI_CMD_INTX_OFF);
		ivshmem_update_intx(ive);
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
		ive->cspace[IVSHMEM_CFG_MSIX_CAP/4] &= ~PCI_MSIX_CTRL_RW_MASK;
		ive->cspace[IVSHMEM_CFG_MSIX_CAP/4] |=
			value & PCI_MSIX_CTRL_RW_MASK;
		if (ivshmem_update_msix(device))
			return PCI_ACCESS_REJECT;
		break;
	case IVSHMEM_CFG_VNDR_CAP / 4:
		ive->cspace[IVSHMEM_CFG_VNDR_CAP/4] &= ~IVSHMEM_CFG_ONESHOT_INT;
		ive->cspace[IVSHMEM_CFG_VNDR_CAP/4] |=
			value & IVSHMEM_CFG_ONESHOT_INT;
		break;
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
	struct ivshmem_endpoint *ive;
	struct ivshmem_link *link;
	unsigned int peer_id, id;
	struct pci_device *peer;

	printk("Adding virtual PCI device %02x:%02x.%x to cell \"%s\"\n",
	       PCI_BDF_PARAMS(dev_info->bdf), cell->config->name);

	if (dev_info->shmem_regions_start + 2 + dev_info->shmem_peers >
	    cell->config->num_memory_regions ||
	    dev_info->num_msix_vectors > IVSHMEM_MSIX_VECTORS)
		return trace_error(-EINVAL);

	for (link = ivshmem_links; link; link = link->next)
		if (link->bdf == dev_info->bdf)
			break;

	id = dev_info->shmem_dev_id;
	if (id >= IVSHMEM_MAX_PEERS)
		return trace_error(-EINVAL);

	if (link) {
		if (link->eps[id].device)
			return trace_error(-EBUSY);

		printk("Shared memory connection established, peer cells:\n");
		for (peer_id = 0; peer_id < IVSHMEM_MAX_PEERS; peer_id++) {
			peer = link->eps[peer_id].device;
			if (peer && peer_id != id)
				printk(" \"%s\"\n", peer->cell->config->name);
		}
	} else {
		link = page_alloc(&mem_pool, PAGES(sizeof(*link)));
		if (!link)
			return -ENOMEM;

		link->bdf = dev_info->bdf;
		link->next = ivshmem_links;
		ivshmem_links = link;
	}

	link->peers++;
	ive = &link->eps[id];

	ive->device = device;
	ive->link = link;
	ive->shmem = jailhouse_cell_mem_regions(cell->config) +
		dev_info->shmem_regions_start;
	if (link->peers == 1)
		memset(ivshmem_map_state_table(ive), 0,
		       dev_info->shmem_peers * sizeof(u32));
	device->ivshmem_endpoint = ive;

	device->cell = cell;
	pci_reset_device(device);

	return 0;
}

void ivshmem_reset(struct pci_device *device)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;
	unsigned int n;

	/*
	 * Hold the spinlock while invalidating in order to synchronize with
	 * any in-flight interrupt from remote sides.
	 */
	spin_lock(&ive->irq_lock);
	ive->int_ctrl_reg = 0;
	memset(&ive->irq_cache, 0, sizeof(ive->irq_cache));
	spin_unlock(&ive->irq_lock);

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
	} else {
		ive->cspace[IVSHMEM_CFG_MSIX_CAP/4] |=
			(device->info->num_msix_vectors - 1) << 16;
		for (n = 0; n < device->info->num_msix_vectors; n++)
			device->msix_vectors[n].masked = 1;
	}

	ive->cspace[IVSHMEM_CFG_SHMEM_STATE_TAB_SZ/4] = (u32)ive->shmem[0].size;

	ive->cspace[IVSHMEM_CFG_SHMEM_RW_SZ/4] = (u32)ive->shmem[1].size;
	ive->cspace[IVSHMEM_CFG_SHMEM_RW_SZ/4 + 1] =
		(u32)(ive->shmem[1].size >> 32);

	ive->cspace[IVSHMEM_CFG_SHMEM_OUTPUT_SZ/4] = (u32)ive->shmem[2].size;
	ive->cspace[IVSHMEM_CFG_SHMEM_OUTPUT_SZ/4 + 1] =
		(u32)(ive->shmem[2].size >> 32);

	ive->cspace[IVSHMEM_CFG_SHMEM_ADDR/4] = (u32)ive->shmem[0].virt_start;
	ive->cspace[IVSHMEM_CFG_SHMEM_ADDR/4 + 1] =
		(u32)(ive->shmem[0].virt_start >> 32);

	ivshmem_write_state(ive, 0);
}

/**
 * Unregister a ivshmem device, typically when the corresponding cell exits.
 * @param device	The device to be stopped.
 *
 */
void ivshmem_exit(struct pci_device *device)
{
	struct ivshmem_endpoint *ive = device->ivshmem_endpoint;
	struct ivshmem_link **linkp;

	/*
	 * Hold the spinlock while invalidating in order to synchronize with
	 * any in-flight interrupt from remote sides.
	 */
	spin_lock(&ive->irq_lock);
	memset(&ive->irq_cache, 0, sizeof(ive->irq_cache));
	spin_unlock(&ive->irq_lock);

	ivshmem_write_state(ive, 0);

	ive->device = NULL;

	if (--ive->link->peers > 0)
		return;

	for (linkp = &ivshmem_links; *linkp; linkp = &(*linkp)->next) {
		if (*linkp != ive->link)
			continue;

		*linkp = ive->link->next;
		page_free(&mem_pool, ive->link, PAGES(sizeof(*ive->link)));
		break;
	}
}
