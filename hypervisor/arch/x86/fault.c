/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/fault.h>

struct exception_frame {
	u64 vector;
	u64 error;
	u64 rip;
	u64 cs;
	u64 flags;
	u64 rsp;
	u64 ss;
};

void exception_handler(struct exception_frame *frame)
{
	panic_printk("FATAL: Jailhouse triggered exception #%d\n",
		     frame->vector);
	if (frame->error != -1)
		panic_printk("Error code: %x\n", frame->error);
	panic_printk("Physical CPU ID: %d\n", phys_processor_id());
	panic_printk("RIP: %p RSP: %p FLAGS: %x\n", frame->rip, frame->rsp,
		     frame->flags);

	panic_stop(NULL);
}
