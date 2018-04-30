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
#include <asm/apic.h>
#include <asm/cat.h>
#include <asm/control.h>
#include <asm/ioapic.h>
#include <asm/iommu.h>
#include <asm/vcpu.h>

struct exception_frame {
	u64 vector;
	u64 error;
	u64 rip;
	u64 cs;
	u64 flags;
	u64 rsp;
	u64 ss;
};

int arch_cell_create(struct cell *cell)
{
	int err;

	err = vcpu_cell_init(cell);
	if (err)
		return err;

	return 0;
}

int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	int err;

	err = vcpu_map_memory_region(cell, mem);
	if (err)
		return err;

	err = iommu_map_memory_region(cell, mem);
	if (err)
		vcpu_unmap_memory_region(cell, mem);
	return err;
}

int arch_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	int err;

	err = iommu_unmap_memory_region(cell, mem);
	if (err)
		return err;

	return vcpu_unmap_memory_region(cell, mem);
}

void arch_flush_cell_vcpu_caches(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu(cpu, cell->cpu_set)
		if (cpu == this_cpu_id()) {
			vcpu_tlb_flush();
		} else {
			per_cpu(cpu)->flush_vcpu_caches = true;
			/* make sure the value is written before we kick
			 * the remote core */
			memory_barrier();
			apic_send_nmi_ipi(public_per_cpu(cpu));
		}
}

void arch_cell_destroy(struct cell *cell)
{
	vcpu_cell_exit(cell);
}

void arch_cell_reset(struct cell *cell)
{
	struct jailhouse_comm_region *comm_region = &cell->comm_page.comm_region;
	unsigned int cpu;

	comm_region->pm_timer_address =
		system_config->platform_info.x86.pm_timer_address;
	comm_region->pci_mmconfig_base =
		system_config->platform_info.pci_mmconfig_base;
	/* comm_region, and hence num_cpus, is zero-initialised */
	for_each_cpu(cpu, cell->cpu_set)
		comm_region->num_cpus++;
	comm_region->tsc_khz = system_config->platform_info.x86.tsc_khz;
	comm_region->apic_khz = system_config->platform_info.x86.apic_khz;

	ioapic_cell_reset(cell);
}

void arch_config_commit(struct cell *cell_added_removed)
{
	iommu_config_commit(cell_added_removed);
	ioapic_config_commit(cell_added_removed);
}

void arch_prepare_shutdown(void)
{
	ioapic_prepare_handover();
	iommu_prepare_shutdown();
}

void arch_suspend_cpu(unsigned int cpu_id)
{
	struct per_cpu *target_data = per_cpu(cpu_id);
	bool target_suspended;

	spin_lock(&target_data->control_lock);

	target_data->suspend_cpu = true;
	target_suspended = target_data->cpu_suspended;

	spin_unlock(&target_data->control_lock);

	if (!target_suspended) {
		/*
		 * Send a maintenance signal (NMI) to the target CPU.
		 * Then, wait for the target CPU to enter the suspended state.
		 * The target CPU, in turn, will leave the guest and handle the
		 * request in the event loop.
		 */
		apic_send_nmi_ipi(&target_data->public);

		while (!target_data->cpu_suspended)
			cpu_relax();
	}
}

void arch_resume_cpu(unsigned int cpu_id)
{
	struct per_cpu *target_data = per_cpu(cpu_id);

	/* take lock to avoid theoretical race with a pending suspension */
	spin_lock(&target_data->control_lock);

	target_data->suspend_cpu = false;

	spin_unlock(&target_data->control_lock);
}

void arch_reset_cpu(unsigned int cpu_id)
{
	per_cpu(cpu_id)->sipi_vector = APIC_BSP_PSEUDO_SIPI;

	arch_resume_cpu(cpu_id);
}

void arch_park_cpu(unsigned int cpu_id)
{
	per_cpu(cpu_id)->init_signaled = true;

	arch_resume_cpu(cpu_id);
}

void x86_send_init_sipi(unsigned int cpu_id, enum x86_init_sipi type,
			int sipi_vector)
{
	struct per_cpu *target_data = per_cpu(cpu_id);
	bool send_nmi = false;

	spin_lock(&target_data->control_lock);

	if (type == X86_INIT) {
		if (!target_data->wait_for_sipi) {
			target_data->init_signaled = true;
			send_nmi = true;
		}
	} else if (target_data->wait_for_sipi) {
		target_data->sipi_vector = sipi_vector;
		send_nmi = true;
	}

	spin_unlock(&target_data->control_lock);

	if (send_nmi)
		apic_send_nmi_ipi(&target_data->public);
}

/* control_lock has to be held */
static void x86_enter_wait_for_sipi(struct per_cpu *cpu_data)
{
	cpu_data->init_signaled = false;
	cpu_data->wait_for_sipi = true;
}

void __attribute__((weak)) cat_update(void)
{
}

void x86_check_events(void)
{
	struct per_cpu *cpu_data = this_cpu_data();
	int sipi_vector = -1;

	spin_lock(&cpu_data->control_lock);

	do {
		if (cpu_data->init_signaled && !cpu_data->suspend_cpu) {
			x86_enter_wait_for_sipi(cpu_data);
			break;
		}

		cpu_data->cpu_suspended = true;

		spin_unlock(&cpu_data->control_lock);

		while (cpu_data->suspend_cpu)
			cpu_relax();

		spin_lock(&cpu_data->control_lock);

		cpu_data->cpu_suspended = false;

		if (cpu_data->sipi_vector >= 0) {
			if (!cpu_data->public.failed) {
				cpu_data->wait_for_sipi = false;
				sipi_vector = cpu_data->sipi_vector;
			}
			cpu_data->sipi_vector = -1;
		}
	} while (cpu_data->init_signaled);

	if (cpu_data->flush_vcpu_caches) {
		cpu_data->flush_vcpu_caches = false;
		vcpu_tlb_flush();
	}

	if (cpu_data->update_cat) {
		cpu_data->update_cat = false;
		cat_update();
	}

	spin_unlock(&cpu_data->control_lock);

	/* wait_for_sipi is only modified on this CPU, so checking outside of
	 * control_lock is fine */
	if (cpu_data->wait_for_sipi) {
		vcpu_park();
	} else if (sipi_vector >= 0) {
		printk("CPU %d received SIPI, vector %x\n", this_cpu_id(),
		       sipi_vector);
		apic_clear();
		vcpu_reset(sipi_vector);
	}

	iommu_check_pending_faults();
}

void __attribute__((noreturn))
x86_exception_handler(struct exception_frame *frame)
{
	panic_printk("FATAL: Jailhouse triggered exception #%lld\n",
		     frame->vector);
	if (frame->error != -1)
		panic_printk("Error code: %llx\n", frame->error);
	panic_printk("Physical CPU ID: %lu\n", phys_processor_id());
	panic_printk("RIP: 0x%016llx RSP: 0x%016llx FLAGS: %llx\n", frame->rip,
		     frame->rsp, frame->flags);
	if (frame->vector == PF_VECTOR)
		panic_printk("CR2: 0x%016lx\n", read_cr2());

	panic_stop();
}

void __attribute__((noreturn)) arch_panic_stop(void)
{
	/* no lock required here as we won't change to false anymore */
	this_cpu_data()->cpu_suspended = true;
	asm volatile("1: hlt; jmp 1b");
	__builtin_unreachable();
}

void arch_panic_park(void)
{
	struct per_cpu *cpu_data = this_cpu_data();

	spin_lock(&cpu_data->control_lock);
	x86_enter_wait_for_sipi(cpu_data);
	spin_unlock(&cpu_data->control_lock);

	vcpu_park();
}
