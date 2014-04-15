/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Ivan Kolchin <ivan.kolchin@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/pci.h>
#include <jailhouse/utils.h>

/* entry for PCI config space whitelist (granting access) */
struct pci_cfg_access {
	u32 reg_num; /** Register number (4-byte aligned) */
	u32 mask; /** Bit set: access allowed */
};

/* --- Whilelist for writing to PCI config space registers --- */
/* Type 1: Endpoints */
static const struct pci_cfg_access endpoint_write_access[] = {
	{ 0x04, 0xffffffff }, /* Command, Status */
	{ 0x0c, 0xff000000 }, /* BIST */
	{ 0x3c, 0x000000ff }, /* Int Line */
};
/* Type 2: Bridges */
static const struct pci_cfg_access bridge_write_access[] = {
	{ 0x04, 0xffffffff }, /* Command, Status */
	{ 0x0c, 0xff000000 }, /* BIST */
	{ 0x3c, 0xffff00ff }, /* Int Line, Bridge Control */
};

/**
 * pci_get_assigned_device() - Look up device owned by a cell
 * @cell:	Owning cell
 * @bdf:	16-bit bus/device/function ID
 *
 * Return: Valid pointer - owns, NULL - doesn't own.
 */
const struct jailhouse_pci_device *
pci_get_assigned_device(const struct cell *cell, u16 bdf)
{
	const struct jailhouse_pci_device *device =
		jailhouse_cell_pci_devices(cell->config);
	u32 n;

	for (n = 0; n < cell->config->num_pci_devices; n++)
		if (((device[n].bus << 8) | device[n].devfn) == bdf)
			return &device[n];

	return NULL;
}

/**
 * pci_cfg_write_allowed() - Check general config space write permission
 * @type:	JAILHOUSE_PCI_TYPE_DEVICE or JAILHOUSE_PCI_TYPE_BRIDGE
 * @reg_num:	Register number (4-byte aligned)
 * @bias:	Bias from register base address in bytes
 * @size:	Access size (1, 2 or 4 bytes)
 *
 * Return: True if writing is allowed, false otherwise.
 */
bool pci_cfg_write_allowed(u32 type, u8 reg_num, unsigned int reg_bias,
			   unsigned int size)
{
	/* initialize list to work around wrong compiler warning */
	const struct pci_cfg_access *list = NULL;
	unsigned int n, len = 0;

	if (type == JAILHOUSE_PCI_TYPE_DEVICE) {
		list = endpoint_write_access;
		len = ARRAY_SIZE(endpoint_write_access);
	} else if (type == JAILHOUSE_PCI_TYPE_BRIDGE) {
		list = bridge_write_access;
		len = ARRAY_SIZE(bridge_write_access);
	}

	for (n = 0; n < len; n++)
		if (list[n].reg_num == reg_num)
			return ((list[n].mask >> (reg_bias * 8)) &
				 BYTE_MASK(size)) == BYTE_MASK(size);

	return false;
}
