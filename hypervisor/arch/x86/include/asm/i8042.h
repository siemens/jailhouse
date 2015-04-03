/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_I8042_H
#define _JAILHOUSE_ASM_I8042_H

#include <jailhouse/types.h>

#define I8042_CMD_REG			0x64
# define I8042_CMD_WRITE_CTRL_PORT	0xd1
# define I8042_CMD_PULSE_CTRL_PORT	0xf0

int i8042_access_handler(u16 port, bool dir_in, unsigned int size);

#endif /* !_JAILHOUSE_ASM_I8042_H */
