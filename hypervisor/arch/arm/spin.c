/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <asm/platform.h>
#include <asm/setup.h>
#include <asm/control.h>

#if HOTPLUG_SPIN == 1
int arch_spin_init(void)
{
	unsigned long mbox = (unsigned long)HOTPLUG_MBOX;
	void *mbox_page = (void *)(mbox & PAGE_MASK);
	int err = arch_map_device(mbox_page, mbox_page, PAGE_SIZE);

	if (err)
		printk("Unable to map spin mbox page\n");

	return err;
}


unsigned long arch_cpu_spin(void)
{
	u32 address;

	/*
	 * This is super-dodgy: we assume nothing wrote to the flag register
	 * since the kernel called smp_prepare_cpus, at initialisation.
	 */
	do {
		wfe();
		address = mmio_read32((void *)HOTPLUG_MBOX);
		cpu_relax();
	} while (address == 0);

	return address;
}

#elif HOTPLUG_PSCI == 1
int arch_spin_init(void)
{
}

unsigned long arch_cpu_spin(void)
{
	/* FIXME: wait for a PSCI hvc */
}
#endif
