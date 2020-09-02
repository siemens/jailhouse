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
#include <jailhouse/printk.h>
#include <asm/control.h>
#include <asm/iommu.h>
#include <asm/psci.h>
#include <asm/smc.h>
#include <asm/smccc.h>

static void enter_cpu_off(struct public_per_cpu *cpu_public)
{
	cpu_public->park = false;
	cpu_public->wait_for_poweron = true;
}

void arm_cpu_park(void)
{
	struct public_per_cpu *cpu_public = this_cpu_public();

	spin_lock(&cpu_public->control_lock);
	enter_cpu_off(cpu_public);
	spin_unlock(&cpu_public->control_lock);

	arm_cpu_reset(0,
		      !!(this_cell()->config->flags & JAILHOUSE_CELL_AARCH32));

	arm_paging_vcpu_init(&parking_pt);
}

void arch_send_event(struct public_per_cpu *target_data)
{
	if (sdei_available)
		smc_arg2(SDEI_EVENT_SIGNAL, 0, target_data->mpidr);
	else
		irqchip_send_sgi(target_data->cpu_id, SGI_EVENT);
}

void arch_reset_cpu(unsigned int cpu_id)
{
	public_per_cpu(cpu_id)->reset = true;

	resume_cpu(cpu_id);
}

void arch_park_cpu(unsigned int cpu_id)
{
	public_per_cpu(cpu_id)->park = true;

	resume_cpu(cpu_id);
}

static void check_events(struct public_per_cpu *cpu_public)
{
	bool reset = false;

	spin_lock(&cpu_public->control_lock);

	while (cpu_public->suspend_cpu) {
		cpu_public->cpu_suspended = true;

		spin_unlock(&cpu_public->control_lock);

		while (cpu_public->suspend_cpu)
			cpu_relax();

		spin_lock(&cpu_public->control_lock);
	}

	cpu_public->cpu_suspended = false;

	if (cpu_public->park) {
		enter_cpu_off(cpu_public);
	} else if (cpu_public->reset) {
		cpu_public->reset = false;
		if (cpu_public->cpu_on_entry != PSCI_INVALID_ADDRESS) {
			cpu_public->wait_for_poweron = false;
			reset = true;
		} else {
			enter_cpu_off(cpu_public);
		}
	}

	if (cpu_public->flush_vcpu_caches) {
		cpu_public->flush_vcpu_caches = false;
		arm_paging_vcpu_flush_tlbs();
	}

	spin_unlock(&cpu_public->control_lock);

	/*
	 * wait_for_poweron is only modified on this CPU, so checking outside of
	 * control_lock is fine.
	 */
	if (cpu_public->wait_for_poweron)
		arm_cpu_park();
	else if (reset)
		arm_cpu_reset(cpu_public->cpu_on_entry,
			      !!(this_cell()->config->flags &
			         JAILHOUSE_CELL_AARCH32));
}

void arch_handle_sgi(u32 irqn, unsigned int count_event)
{
	struct public_per_cpu *cpu_public = this_cpu_public();

	switch (irqn) {
	case SGI_INJECT:
		cpu_public->stats[JAILHOUSE_CPU_STAT_VMEXITS_VSGI] +=
			count_event;
		irqchip_inject_pending();
		break;
	case SGI_EVENT:
		cpu_public->stats[JAILHOUSE_CPU_STAT_VMEXITS_MANAGEMENT] +=
			count_event;
		check_events(cpu_public);
		break;
	default:
		printk("WARN: unknown SGI received %d\n", irqn);
	}
}

/*
 * Handle the maintenance interrupt, the rest is injected into the cell.
 * Return true when the IRQ has been handled by the hyp.
 */
bool arch_handle_phys_irq(u32 irqn, unsigned int count_event)
{
	struct public_per_cpu *cpu_public = this_cpu_public();

	if (irqn == system_config->platform_info.arm.maintenance_irq) {
		cpu_public->stats[JAILHOUSE_CPU_STAT_VMEXITS_MAINTENANCE] +=
			count_event;
		irqchip_inject_pending();

		return true;
	}

	cpu_public->stats[JAILHOUSE_CPU_STAT_VMEXITS_VIRQ] += count_event;
	irqchip_set_pending(cpu_public, irqn);

	return false;
}

int arch_cell_create(struct cell *cell)
{
	return arm_paging_cell_init(cell);
}

void arch_cell_reset(struct cell *cell)
{
	unsigned int first = first_cpu(cell->cpu_set);
	unsigned int cpu;
	struct jailhouse_comm_region *comm_region =
		&cell->comm_page.comm_region;

	/* Place platform specific information inside comm_region */
	comm_region->gic_version = system_config->platform_info.arm.gic_version;
	comm_region->gicd_base = system_config->platform_info.arm.gicd_base;
	comm_region->gicc_base = system_config->platform_info.arm.gicc_base;
	comm_region->gicr_base = system_config->platform_info.arm.gicr_base;
	comm_region->vpci_irq_base = cell->config->vpci_irq_base;

	/*
	 * All CPUs but the first are initially suspended.  The first CPU
	 * starts at cpu_reset_address, defined in the cell configuration.
	 */
	public_per_cpu(first)->cpu_on_entry = cell->config->cpu_reset_address;
	for_each_cpu_except(cpu, cell->cpu_set, first)
		public_per_cpu(cpu)->cpu_on_entry = PSCI_INVALID_ADDRESS;

	arm_cell_dcaches_flush(cell, DCACHE_INVALIDATE);

	irqchip_cell_reset(cell);
}

void arch_cell_destroy(struct cell *cell)
{
	unsigned int cpu;

	arm_cell_dcaches_flush(cell, DCACHE_INVALIDATE);

	/* All CPUs are handed back to the root cell in suspended mode. */
	for_each_cpu(cpu, cell->cpu_set)
		public_per_cpu(cpu)->cpu_on_entry = PSCI_INVALID_ADDRESS;

	arm_paging_cell_destroy(cell);
}

/* Note: only supports synchronous flushing as triggered by config_commit! */
void arch_flush_cell_vcpu_caches(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu(cpu, cell->cpu_set)
		if (cpu == this_cpu_id())
			arm_paging_vcpu_flush_tlbs();
		else
			public_per_cpu(cpu)->flush_vcpu_caches = true;
}

void arch_config_commit(struct cell *cell_added_removed)
{
	irqchip_config_commit(cell_added_removed);
	iommu_config_commit(cell_added_removed);
}

void __attribute__((noreturn)) arch_panic_stop(void)
{
	asm volatile ("1: wfi; b 1b");
	__builtin_unreachable();
}

#ifndef CONFIG_CRASH_CELL_ON_PANIC
void arch_panic_park(void) __attribute__((alias("arm_cpu_park")));
#endif

void arch_prepare_shutdown(void)
{
}
