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

#include <jailhouse/printk.h>
#include <jailhouse/utils.h>
#include <asm/i8042.h>
#include <asm/io.h>
#include <jailhouse/percpu.h>

#include <jailhouse/cell-config.h>

int i8042_access_handler(u16 port, bool dir_in, unsigned int size)
{
	union registers *guest_regs = &this_cpu_data()->guest_regs;
	u8 val;

	if (port == I8042_CMD_REG && this_cell()->arch.pio_i8042_allowed) {
		if (size != 1)
			goto invalid_access;
		if (dir_in) {
			guest_regs->rax &= ~BYTE_MASK(1);
			guest_regs->rax |= inb(I8042_CMD_REG);
		} else {
			val = (u8)guest_regs->rax;
			if (val == I8042_CMD_WRITE_CTRL_PORT ||
			    (val & I8042_CMD_PULSE_CTRL_PORT) ==
			    I8042_CMD_PULSE_CTRL_PORT)
				goto invalid_access;
			outb(val, I8042_CMD_REG);
		}
		return 1;
	}
	return 0;

invalid_access:
	panic_printk("FATAL: Invalid write to i8042 controller port\n");
	return -1;
}
