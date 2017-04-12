/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_DRIVER_SYSFS_H
#define _JAILHOUSE_DRIVER_SYSFS_H

#include <linux/device.h>

int jailhouse_sysfs_cell_create(struct cell *cell);
void jailhouse_sysfs_cell_register(struct cell *cell);
void jailhouse_sysfs_cell_delete(struct cell *cell);

int jailhouse_sysfs_core_init(struct device *dev, size_t hypervisor_size);
void jailhouse_sysfs_core_exit(struct device *dev);
int jailhouse_sysfs_init(struct device *dev);
void jailhouse_sysfs_exit(struct device *dev);

#endif /* !_JAILHOUSE_DRIVER_SYSFS_H */
