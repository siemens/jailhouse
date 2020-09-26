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
#include <asm/smccc.h>
#include <asm/traps.h>

static long psci_emulate_cpu_on(struct trap_context *ctx)
{
	u64 mask = SMCCC_IS_CONV_64(ctx->regs[0]) ? (u64)-1L : (u32)-1;
	struct public_per_cpu *target_data;
	bool kick_cpu = false;
	unsigned int cpu;
	long result;

	cpu = arm_cpu_by_mpidr(this_cell(), ctx->regs[1] & mask);
	if (cpu == INVALID_CPU_ID)
		/* Virtual id not in set */
		return PSCI_DENIED;

	target_data = public_per_cpu(cpu);

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

	/*
	 * The unlock has memory barrier semantic on ARM v7 and v8. Therefore
	 * the changes to target_data will be visible when sending the kick
	 * below.
	 */
	spin_unlock(&target_data->control_lock);

	if (kick_cpu)
		arch_send_event(target_data);

	return result;
}

static long psci_emulate_affinity_info(struct trap_context *ctx)
{
	unsigned int cpu = arm_cpu_by_mpidr(this_cell(), ctx->regs[1]);

	if (cpu == INVALID_CPU_ID)
		/* Virtual id not in set */
		return PSCI_DENIED;

	return public_per_cpu(cpu)->wait_for_poweron ?
		PSCI_CPU_IS_OFF : PSCI_CPU_IS_ON;
}

static long psci_emulate_features_info(struct trap_context *ctx)
{
	switch (ctx->regs[1]) {
	case PSCI_0_2_FN_VERSION:
	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
	case PSCI_0_2_FN_CPU_OFF:
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN64_AFFINITY_INFO:
	case PSCI_1_0_FN_FEATURES:
	case SMCCC_VERSION:
		return PSCI_SUCCESS;

	default:
		return PSCI_NOT_SUPPORTED;
	}
}

long psci_dispatch(struct trap_context *ctx)
{
	this_cpu_public()->stats[JAILHOUSE_CPU_STAT_VMEXITS_PSCI]++;

	switch (ctx->regs[0]) {
	case PSCI_0_2_FN_VERSION:
		return PSCI_VERSION(1, 1);

	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
		/*
		 * Note: We ignore the power_state parameter and always perform
		 * a context-preserving suspend. This is legal according to
		 * PSCI.
		 */
		if (sdei_available) {
			arm_cpu_passthru_suspend();
		} else if (!irqchip_has_pending_irqs()) {
			asm volatile("wfi" : : : "memory");
			irqchip_handle_irq();
		}
		return PSCI_SUCCESS;

	case PSCI_0_2_FN_CPU_OFF:
	case PSCI_CPU_OFF_V0_1_UBOOT:
		arm_cpu_park();
		return 0; /* never returned to the PSCI caller */

	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
	case PSCI_CPU_ON_V0_1_UBOOT:
		return psci_emulate_cpu_on(ctx);

	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN64_AFFINITY_INFO:
		return psci_emulate_affinity_info(ctx);

	case PSCI_1_0_FN_FEATURES:
		return psci_emulate_features_info(ctx);

	default:
		return PSCI_NOT_SUPPORTED;
	}
}
