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

#include "cell.h"

enum {
	JAILHOUSE_PCI_ACTION_ADD, JAILHOUSE_PCI_ACTION_DEL,
	JAILHOUSE_PCI_ACTION_CLAIM, JAILHOUSE_PCI_ACTION_RELEASE,
};

#ifdef CONFIG_PCI

void jailhouse_pci_do_all_devices(struct cell *cell, unsigned int type,
				  unsigned int action);
int jailhouse_pci_cell_setup(struct cell *cell,
			     const struct jailhouse_cell_desc *cell_desc);
void jailhouse_pci_cell_cleanup(struct cell *cell);
void jailhouse_pci_virtual_root_devices_add(struct jailhouse_system *config);
void jailhouse_pci_virtual_root_devices_remove(void);
int jailhouse_pci_register(void);
void jailhouse_pci_unregister(void);

#else /* !CONFIG_PCI */

static inline void
jailhouse_pci_do_all_devices(struct cell *cell, unsigned int type,
			     unsigned int action)
{
}

static inline int
jailhouse_pci_cell_setup(struct cell *cell,
			 const struct jailhouse_cell_desc *cell_desc)
{
	return 0;
}

static inline void jailhouse_pci_cell_cleanup(struct cell *cell)
{
}

static inline void
jailhouse_pci_virtual_root_devices_add(struct jailhouse_system *config)
{
}

static inline void jailhouse_pci_virtual_root_devices_remove(void)
{
}

static inline int jailhouse_pci_register(void)
{
	return 0;
}

static inline void jailhouse_pci_unregister(void)
{
}

#endif /* !CONFIG_PCI */
