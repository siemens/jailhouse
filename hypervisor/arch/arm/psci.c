/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/control.h>
#include <asm/percpu.h>
#include <asm/psci.h>
#include <asm/traps.h>
#include <jailhouse/control.h>

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

static long psci_emulate_cpu_on(struct per_cpu *cpu_data,
				struct trap_context *ctx)
{
	struct per_cpu *target_data;
	bool kick_cpu = false;
	unsigned int cpu;
	long result;

	cpu = arm_cpu_by_mpidr(cpu_data->cell, ctx->regs[1]);
	if (cpu == -1)
		/* Virtual id not in set */
		return PSCI_DENIED;

	target_data = per_cpu(cpu);

	spin_lock(&target_data->control_lock);

	if (target_data->wait_for_poweron) {
		target_data->cpu_on_entry = ctx->regs[2];
		target_data->cpu_on_context = ctx->regs[3];
		target_data->reset = true;
		kick_cpu = true;

		result = PSCI_SUCCESS;
	} else {
		result = PSCI_ALREADY_ON;
	}

	spin_unlock(&target_data->control_lock);

	if (kick_cpu)
		arm_cpu_kick(cpu);

	return result;
}

static long psci_emulate_affinity_info(struct per_cpu *cpu_data,
				       struct trap_context *ctx)
{
	unsigned int cpu = arm_cpu_by_mpidr(cpu_data->cell, ctx->regs[1]);

	if (cpu == -1)
		/* Virtual id not in set */
		return PSCI_DENIED;

	return per_cpu(cpu)->wait_for_poweron ?
		PSCI_CPU_IS_OFF : PSCI_CPU_IS_ON;
}

long psci_dispatch(struct trap_context *ctx)
{
	struct per_cpu *cpu_data = this_cpu_data();
	u32 function_id = ctx->regs[0];

	switch (function_id) {
	case PSCI_VERSION:
		/* Major[31:16], minor[15:0] */
		return 2;

	case PSCI_CPU_OFF:
	case PSCI_CPU_OFF_V0_1_UBOOT:
		arm_cpu_park();
		return 0;

	case PSCI_CPU_ON_32:
	case PSCI_CPU_ON_V0_1_UBOOT:
		return psci_emulate_cpu_on(cpu_data, ctx);

	case PSCI_AFFINITY_INFO_32:
		return psci_emulate_affinity_info(cpu_data, ctx);

	default:
		return PSCI_NOT_SUPPORTED;
	}
}
