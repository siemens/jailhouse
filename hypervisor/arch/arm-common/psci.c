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

#include <jailhouse/control.h>
#include <asm/control.h>
#include <asm/psci.h>
#include <asm/traps.h>

static long psci_emulate_cpu_on(struct trap_context *ctx)
{
	unsigned long mask = IS_PSCI_64(ctx->regs[0]) ? (u64)-1L : (u32)-1;
	struct per_cpu *target_data;
	bool kick_cpu = false;
	unsigned int cpu;
	long result;

	cpu = arm_cpu_by_mpidr(this_cell(), ctx->regs[1] & mask);
	if (cpu == -1)
		/* Virtual id not in set */
		return PSCI_DENIED;

	target_data = per_cpu(cpu);

	spin_lock(&target_data->control_lock);

	if (target_data->wait_for_poweron) {
		target_data->cpu_on_entry = ctx->regs[2] & mask;
		target_data->cpu_on_context = ctx->regs[3] & mask;
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

static long psci_emulate_affinity_info(struct trap_context *ctx)
{
	unsigned int cpu = arm_cpu_by_mpidr(this_cell(), ctx->regs[1]);

	if (cpu == -1)
		/* Virtual id not in set */
		return PSCI_DENIED;

	return per_cpu(cpu)->wait_for_poweron ?
		PSCI_CPU_IS_OFF : PSCI_CPU_IS_ON;
}

long psci_dispatch(struct trap_context *ctx)
{
	u32 function_id = ctx->regs[0];

	this_cpu_data()->stats[JAILHOUSE_CPU_STAT_VMEXITS_PSCI]++;

	switch (function_id) {
	case PSCI_VERSION:
		/* Major[31:16], minor[15:0] */
		return 2;

	case PSCI_CPU_SUSPEND_32:
	case PSCI_CPU_SUSPEND_64:
		if (!irqchip_has_pending_irqs()) {
			asm volatile("wfi" : : : "memory");
			irqchip_handle_irq();
		}
		return 0;

	case PSCI_CPU_OFF:
	case PSCI_CPU_OFF_V0_1_UBOOT:
		arm_cpu_park();
		return 0;

	case PSCI_CPU_ON_32:
	case PSCI_CPU_ON_64:
	case PSCI_CPU_ON_V0_1_UBOOT:
		return psci_emulate_cpu_on(ctx);

	case PSCI_AFFINITY_INFO_32:
	case PSCI_AFFINITY_INFO_64:
		return psci_emulate_affinity_info(ctx);

	default:
		return PSCI_NOT_SUPPORTED;
	}
}
