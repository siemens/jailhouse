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

#include <asm/control.h>
#include <asm/psci.h>
#include <asm/traps.h>

void _psci_cpu_off(struct psci_mbox *);
long _psci_cpu_on(struct psci_mbox *, unsigned long, unsigned long);
void _psci_suspend(struct psci_mbox *, unsigned long *address);
void _psci_suspend_return(void);

void psci_cpu_off(struct per_cpu *cpu_data)
{
	_psci_cpu_off(&cpu_data->psci_mbox);
}

long psci_cpu_on(unsigned int target, unsigned long entry,
			unsigned long context)
{
	struct per_cpu *cpu_data = per_cpu(target);
	struct psci_mbox *mbox = &cpu_data->psci_mbox;

	return _psci_cpu_on(mbox, entry, context);
}

/*
 * Not a real psci_cpu_suspend implementation. Only used to semantically
 * differentiate from `cpu_off'. Return is done via psci_resume.
 */
void psci_suspend(struct per_cpu *cpu_data)
{
	psci_cpu_off(cpu_data);
}

long psci_resume(unsigned int target)
{
	psci_wait_cpu_stopped(target);
	return psci_cpu_on(target, (unsigned long)&_psci_suspend_return, 0);
}

bool psci_cpu_stopped(unsigned int cpu_id)
{
	return per_cpu(cpu_id)->psci_mbox.entry == PSCI_INVALID_ADDRESS;
}

long psci_try_resume(unsigned int cpu_id)
{
	if (psci_cpu_stopped(cpu_id))
		return psci_resume(cpu_id);

	return -EBUSY;
}

int psci_wait_cpu_stopped(unsigned int cpu_id)
{
	/* FIXME: add a delay */
	do {
		if (psci_cpu_stopped(cpu_id))
			return 0;
		cpu_relax();
	} while (1);

	return -EBUSY;
}
