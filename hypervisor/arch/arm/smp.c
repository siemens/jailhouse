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

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <asm/control.h>
#include <asm/psci.h>
#include <asm/setup.h>
#include <asm/smp.h>
#include <asm/traps.h>

int arch_generic_smp_init(unsigned long mbox)
{
	void *mbox_page = (void *)(mbox & PAGE_MASK);
	int err = arch_map_device(mbox_page, mbox_page, PAGE_SIZE);

	if (err)
		printk("Unable to map spin mbox page\n");

	return err;
}

unsigned long arch_generic_smp_spin(unsigned long mbox)
{
	/*
	 * This is super-dodgy: we assume nothing wrote to the flag register
	 * since the kernel called smp_prepare_cpus, at initialisation.
	 */
	return mmio_read32((void *)mbox);
}

int arch_generic_smp_mmio(struct per_cpu *cpu_data, struct mmio_access *access,
			  unsigned long mbox)
{
	unsigned int cpu;
	unsigned long mbox_page = mbox & PAGE_MASK;

	if (access->addr < mbox_page || access->addr >= mbox_page + PAGE_SIZE)
		return TRAP_UNHANDLED;

	if (access->addr != mbox || !access->is_write)
		/* Ignore all other accesses */
		return TRAP_HANDLED;

	for_each_cpu_except(cpu, cpu_data->cell->cpu_set, cpu_data->cpu_id) {
		per_cpu(cpu)->guest_mbox.entry = access->val;
		psci_try_resume(cpu);
	}

	return TRAP_HANDLED;
}

unsigned long arch_smp_spin(struct per_cpu *cpu_data, struct smp_ops *ops)
{
	/*
	 * Hotplugging CPU0 is not currently supported. It is always assumed to
	 * be the primary CPU. This is consistent with the linux behavior on
	 * most platforms.
	 * The guest image always starts at virtual address 0.
	 */
	if (cpu_data->virt_id == 0)
		return 0;

	return ops->cpu_spin(cpu_data);
}

int arch_smp_mmio_access(struct per_cpu *cpu_data, struct mmio_access *access)
{
	struct smp_ops *smp_ops = cpu_data->cell->arch.smp;

	if (smp_ops->mmio_handler)
		return smp_ops->mmio_handler(cpu_data, access);

	return TRAP_UNHANDLED;
}
