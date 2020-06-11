/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Henning Schild <henning.schild@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/list.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#ifdef CONFIG_OF_OVERLAY
#include <dt-bindings/interrupt-controller/arm-gic.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
#define of_overlay_apply(overlay, id)	(*id = of_overlay_create(overlay))
#define of_overlay_remove(id)		of_overlay_destroy(*id)
#endif

#include "pci.h"

struct claimed_dev {
	struct list_head list;
	struct pci_dev *dev;
};

static LIST_HEAD(claimed_devs);
static DEFINE_SPINLOCK(claimed_devs_lock);

static int jailhouse_pci_stub_probe(struct pci_dev *dev,
				    const struct pci_device_id *id)
{
	struct claimed_dev *claimed_dev;
	int ret = -ENODEV;

	spin_lock(&claimed_devs_lock);
	list_for_each_entry(claimed_dev, &claimed_devs, list) {
		if (claimed_dev->dev == dev) {
			dev_info(&dev->dev,
				 "claimed for use in non-root cell\n");
			ret = 0;
			break;
		}
	}
	spin_unlock(&claimed_devs_lock);
	return ret;
}

/**
 * When assigning PCI devices to cells we need to make sure Linux in the
 * root-cell does not use them anymore. As long as devices are assigned to
 * other cells the root-cell must not access the devices.
 * Unfortunately we can not just use PCI hotplugging to remove/re-add
 * devices at runtime. Linux will reprogram the BARs and locate ressources
 * where we do not expect/allow them.
 * So Jailhouse acts as a PCI dummy driver and claims the devices while
 * other cells use them. When creating a cell devices will be unbound from
 * their drivers and bound to jailhouse. When a cell is destroyed jailhouse
 * will release its devices. When jailhouse is disabled it will release all
 * assigned devices.
 * @see jailhouse_pci_claim_release
 *
 * When releasing devices they will not be bound to any driver anymore and
 * from Linuxs point of view the jailhouse dummy will still look like a
 * valid driver. Assignment back to the original driver has to be done
 * manually.
 */
static struct pci_driver jailhouse_pci_stub_driver = {
	.name		= "jailhouse-pci-stub",
	.id_table	= NULL,
	.probe		= jailhouse_pci_stub_probe,
};

static void jailhouse_pci_add_device(const struct jailhouse_pci_device *dev)
{
	int num;
	struct pci_bus *bus;

	bus = pci_find_bus(dev->domain, PCI_BUS_NUM(dev->bdf));
	if (bus) {
		num = pci_scan_slot(bus, dev->bdf & 0xff);
		if (num) {
			pci_lock_rescan_remove();
			pci_bus_assign_resources(bus);
			pci_bus_add_devices(bus);
			pci_unlock_rescan_remove();
		}
	}
}

static void jailhouse_pci_remove_device(const struct jailhouse_pci_device *dev)
{
	struct pci_dev *l_dev;

	l_dev = pci_get_domain_bus_and_slot(dev->domain, PCI_BUS_NUM(dev->bdf),
					    dev->bdf & 0xff);
	if (l_dev) {
		pci_stop_and_remove_bus_device_locked(l_dev);
		pci_dev_put(l_dev);
	}
}

static void jailhouse_pci_claim_release(const struct jailhouse_pci_device *dev,
					unsigned int action)
{
	int err;
	struct pci_bus *bus;
	struct pci_dev *l_dev;
	struct device_driver *drv;
	struct claimed_dev *claimed_dev, *tmp_claimed_dev;

	bus = pci_find_bus(dev->domain, PCI_BUS_NUM(dev->bdf));
	if (!bus)
		return;
	l_dev = pci_get_slot(bus, dev->bdf & 0xff);
	if (!l_dev)
		return;
	drv = l_dev->dev.driver;

	if (action == JAILHOUSE_PCI_ACTION_CLAIM) {
		if (drv == &jailhouse_pci_stub_driver.driver)
			return;
		claimed_dev = kmalloc(sizeof(*claimed_dev), GFP_KERNEL);
		if (!claimed_dev) {
			dev_warn(&l_dev->dev, "failed to allocate list entry, "
			         "the driver might not work as expected\n");
			return;
		}
		claimed_dev->dev = l_dev;
		spin_lock(&claimed_devs_lock);
		list_add(&(claimed_dev->list), &claimed_devs);
		spin_unlock(&claimed_devs_lock);
		device_release_driver(&l_dev->dev);
		err = pci_add_dynid(&jailhouse_pci_stub_driver, l_dev->vendor,
				    l_dev->device, l_dev->subsystem_vendor,
				    l_dev->subsystem_device, l_dev->class,
				    0, 0);
		if (err)
			dev_warn(&l_dev->dev, "failed to add dynamic id (%d)\n",
			         err);
	} else {
		/* on "jailhouse disable" we will come here with the
		 * request to release all pci devices, so check the driver */
		if (drv == &jailhouse_pci_stub_driver.driver)
			device_release_driver(&l_dev->dev);
		list_for_each_entry_safe(claimed_dev, tmp_claimed_dev,
					 &claimed_devs, list)
			if (claimed_dev->dev == l_dev) {
				spin_lock(&claimed_devs_lock);
				list_del(&(claimed_dev->list));
				spin_unlock(&claimed_devs_lock);
				kfree(claimed_dev);
				break;
			}
	}
}

/**
 * Register jailhouse as a PCI device driver so it can claim assigned devices.
 *
 * @return 0 on success, or error code
 */
int jailhouse_pci_register(void)
{
	return pci_register_driver(&jailhouse_pci_stub_driver);
}

/**
 * Unegister jailhouse as a PCI device driver.
 */
void jailhouse_pci_unregister(void)
{
	pci_unregister_driver(&jailhouse_pci_stub_driver);
}

/**
 * Apply the given action to all of the cells PCI devices matching the given
 * type.
 * @param cell		the cell containing the PCI devices
 * @param type		PCI device type (JAILHOUSE_PCI_TYPE_*)
 * @param action	action (JAILHOUSE_PCI_ACTION_*)
 */
void jailhouse_pci_do_all_devices(struct cell *cell, unsigned int type,
				  unsigned int action)
{
	unsigned int n;
	const struct jailhouse_pci_device *dev;

	dev = cell->pci_devices;
	for (n = cell->num_pci_devices; n > 0; n--) {
		if (dev->type == type) {
			switch(action) {
			case JAILHOUSE_PCI_ACTION_ADD:
				jailhouse_pci_add_device(dev);
				break;
			case JAILHOUSE_PCI_ACTION_DEL:
				jailhouse_pci_remove_device(dev);
				break;
			default:
				jailhouse_pci_claim_release(dev, action);
			}
		}
		dev++;
	}
}

int jailhouse_pci_cell_setup(struct cell *cell,
			     const struct jailhouse_cell_desc *cell_desc)
{
	if (cell_desc->num_pci_devices == 0)
		/* cell is zero-initialized, no need to set pci fields */
		return 0;

	if (cell_desc->num_pci_devices >=
	    ULONG_MAX / sizeof(struct jailhouse_pci_device))
		return -EINVAL;

	cell->num_pci_devices = cell_desc->num_pci_devices;
	cell->pci_devices = vmalloc(sizeof(struct jailhouse_pci_device) *
				    cell->num_pci_devices);
	if (!cell->pci_devices)
		return -ENOMEM;

	memcpy(cell->pci_devices,
	       jailhouse_cell_pci_devices(cell_desc),
	       sizeof(struct jailhouse_pci_device) * cell->num_pci_devices);

	return 0;
}

void jailhouse_pci_cell_cleanup(struct cell *cell)
{
	vfree(cell->pci_devices);
}

#ifdef CONFIG_OF_OVERLAY
extern u8 __dtb_vpci_template_begin[], __dtb_vpci_template_end[];

static int overlay_id;
static bool overlay_applied;
static struct of_changeset overlay_changeset;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,17,0)
static struct device_node *overlay;
#endif

static void free_prop(struct property *prop)
{
	if (!prop)
		return;

	kfree(prop->name);
	kfree(prop->value);
	kfree(prop);
}

static struct property *alloc_prop(const char *name, int length)
{
	struct property *prop;

	prop = kzalloc(sizeof(*prop), GFP_KERNEL);
	if (!prop)
		return NULL;

	prop->name = kstrdup(name, GFP_KERNEL);
	prop->length = length;
	prop->value = kzalloc(length, GFP_KERNEL);
	if (!prop->name || !prop->value) {
		free_prop(prop);
		return NULL;
	}

	return prop;
}

static unsigned int count_ivshmem_devices(struct cell *cell)
{
	const struct jailhouse_pci_device *dev = cell->pci_devices;
	unsigned int n, count = 0;

	for (n = cell->num_pci_devices; n > 0; n--, dev++)
		if (dev->type == JAILHOUSE_PCI_TYPE_IVSHMEM)
			count++;
	return count;
}

static const struct of_device_id gic_of_match[] = {
	{ .compatible = "arm,cortex-a15-gic", },
	{ .compatible = "arm,cortex-a7-gic", },
	{ .compatible = "arm,gic-400", },
	{ .compatible = "arm,gic-v3", },
	{},
};

static bool create_vpci_of_overlay(struct jailhouse_system *config)
{
	u32 address_cells, size_cells, gic_address_cells, gic_phandle;
	struct device_node *vpci_node = NULL;
	struct device_node *root, *gic;
	struct property *prop = NULL;
	unsigned int n, cell;
	u64 base_addr;
	u32 *prop_val;

	root = of_find_node_by_path("/");
	if (!root)
		return false;

	if (of_property_read_u32(root, "#address-cells", &address_cells) < 0 ||
	    of_property_read_u32(root, "#size-cells", &size_cells) < 0) {
		of_node_put(root);
		return false;
	}

	of_node_put(root);

	gic = of_find_matching_node(NULL, gic_of_match);
	if (!gic)
		return false;

	if (of_property_read_u32(gic, "#address-cells", &gic_address_cells) < 0)
		gic_address_cells = 0;
	gic_phandle = gic->phandle;

	of_node_put(gic);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
	if (of_overlay_fdt_apply(__dtb_vpci_template_begin,
			__dtb_vpci_template_end - __dtb_vpci_template_begin,
			&overlay_id) < 0)
		return false;

#else /* < 4.17 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
	of_fdt_unflatten_tree((const void *)__dtb_vpci_template_begin, NULL,
			      &overlay);
#else /* < 4.7 */
	of_fdt_unflatten_tree((const void *)__dtb_vpci_template_begin, &overlay);
#endif /* < 4.7 */
	if (!overlay)
		goto out_compat;

	of_node_set_flag(overlay, OF_DETACHED);

	if (of_overlay_apply(overlay, &overlay_id) < 0)
		goto out_compat;
#endif /* < 4.17 */

	of_changeset_init(&overlay_changeset);

	vpci_node = of_find_node_by_path("/pci@0");
	if (!vpci_node)
		goto out;

	/*
	 * Set only here to suppress warnings of dtc when compiling the
	 * incomplete overlay.
	 */
	prop = alloc_prop("device_type", 4);
	if (!prop)
		goto out;
	strcpy(prop->value, "pci");

	if (of_changeset_add_property(&overlay_changeset, vpci_node, prop) < 0)
		goto out;

	if (config->platform_info.pci_domain != (u16)-1) {
		prop = alloc_prop("linux,pci-domain", sizeof(u32));
		if (!prop)
			goto out;

		prop_val = prop->value;
		prop_val[0] = cpu_to_be32(config->platform_info.pci_domain);

		if (of_changeset_add_property(&overlay_changeset, vpci_node,
					      prop) < 0)
			goto out;
	}

	prop = alloc_prop("interrupt-map",
			  sizeof(u32) * (8 + gic_address_cells) * 4);
	if (!prop)
		goto out;

	prop_val = prop->value;
	for (n = 0, cell = 0; n < 4; n++) {
		cell += 3;				/* match addr (0) */
		prop_val[cell++] = cpu_to_be32(n + 1);	/* match addr */
		prop_val[cell++] = cpu_to_be32(gic_phandle);
		cell += gic_address_cells;		/* parent addr (0) */
		prop_val[cell++] = cpu_to_be32(GIC_SPI);
		prop_val[cell++] =
			cpu_to_be32(config->root_cell.vpci_irq_base + n);
		prop_val[cell++] = cpu_to_be32(IRQ_TYPE_EDGE_RISING);
	}

	if (of_changeset_add_property(&overlay_changeset, vpci_node, prop) < 0)
		goto out;

	prop = alloc_prop("reg", sizeof(u32) * (address_cells + size_cells));
	if (!prop)
		goto out;

	/* Set the MMCONFIG base address of the host controller. */
	base_addr = config->platform_info.pci_mmconfig_base;
	prop_val = prop->value;
	if (address_cells > 1)
		*prop_val++ = cpu_to_be32(base_addr >> 32);
	*prop_val++ = cpu_to_be32(base_addr);
	if (size_cells > 1)
		*prop_val++ = 0;
	*prop_val = cpu_to_be32(0x100000);

	if (of_changeset_add_property(&overlay_changeset, vpci_node, prop) < 0)
		goto out;

	prop = alloc_prop("ranges", sizeof(u32) * (5 + address_cells));
	if (!prop)
		goto out;

	/*
	 * Locate the resource window right after MMCONFIG, which is only
	 * covering one bus. Reserve 2 pages per virtual shared memory device.
	 */
	base_addr += 0x100000;
	prop_val = prop->value;
	*prop_val++ = cpu_to_be32(0x02000000);
	*prop_val++ = cpu_to_be32(base_addr >> 32);
	*prop_val++ = cpu_to_be32(base_addr);
	if (address_cells > 1)
		*prop_val++ = cpu_to_be32(base_addr >> 32);
	*prop_val++ = cpu_to_be32(base_addr);
	*prop_val++ = 0;
	*prop_val = cpu_to_be32(count_ivshmem_devices(root_cell) *
				2 * PAGE_SIZE);

	if (of_changeset_add_property(&overlay_changeset, vpci_node, prop) < 0)
		goto out;

	prop = alloc_prop("status", 3);
	if (!prop)
		goto out;
	strcpy(prop->value, "ok");

	if (of_changeset_update_property(&overlay_changeset, vpci_node,
					 prop) < 0)
		goto out;
	prop = NULL;

	if (of_changeset_apply(&overlay_changeset) < 0)
		goto out;

	overlay_applied = true;

out:
	free_prop(prop);
	of_node_put(vpci_node);
	if (!overlay_applied) {
		struct of_changeset_entry *ce;

		list_for_each_entry(ce, &overlay_changeset.entries, node)
			free_prop(ce->prop);
		of_changeset_destroy(&overlay_changeset);
		of_overlay_remove(&overlay_id);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,17,0)
out_compat:
		kfree(overlay);
		overlay = NULL;
#endif
	}

	return overlay_applied;
}

static void destroy_vpci_of_overlay(void)
{
	if (overlay_applied) {
		of_changeset_revert(&overlay_changeset);
		of_changeset_destroy(&overlay_changeset);
		of_overlay_remove(&overlay_id);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,17,0)
		kfree(overlay);
		overlay = NULL;
#endif
		overlay_applied = false;
	}
}
#else /* !CONFIG_OF_OVERLAY */
static bool create_vpci_of_overlay(struct jailhouse_system *config)
{
	pr_warn("jailhouse: CONFIG_OF_OVERLAY disabled\n");
	return false;
}

static void destroy_vpci_of_overlay(void)
{
}
#endif

void jailhouse_pci_virtual_root_devices_add(struct jailhouse_system *config)
{
	if (config->platform_info.pci_is_virtual &&
	    !create_vpci_of_overlay(config)) {
		pr_warn("jailhouse: failed to add virtual host controller\n");
		return;
	}

	jailhouse_pci_do_all_devices(root_cell, JAILHOUSE_PCI_TYPE_IVSHMEM,
				     JAILHOUSE_PCI_ACTION_ADD);
}

void jailhouse_pci_virtual_root_devices_remove(void)
{
	jailhouse_pci_do_all_devices(root_cell, JAILHOUSE_PCI_TYPE_IVSHMEM,
				     JAILHOUSE_PCI_ACTION_DEL);

	destroy_vpci_of_overlay();
}
