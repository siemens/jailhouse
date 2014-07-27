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
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/apic.h>
#include <asm/control.h>
#include <asm/ioapic.h>
#include <asm/vmx.h>
#include <asm/vtd.h>

struct exception_frame {
	u64 vector;
	u64 error;
	u64 rip;
	u64 cs;
	u64 flags;
	u64 rsp;
	u64 ss;
};

int arch_cell_create(struct per_cpu *cpu_data, struct cell *cell)
{
	int err;

	err = vmx_cell_init(cell);
	if (err)
		return err;

	err = vtd_cell_init(cell);
	if (err)
		goto error_vmx_exit;

	err = pci_cell_init(cell);
	if (err)
		goto error_vtd_exit;

	ioapic_cell_init(cell);

	cell->comm_page.comm_region.pm_timer_address =
		system_config->platform_info.x86.pm_timer_address;

	return 0;

error_vtd_exit:
	vtd_cell_exit(cell);
error_vmx_exit:
	vmx_cell_exit(cell);
	return err;
}

int arch_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	int err;

	err = vmx_map_memory_region(cell, mem);
	if (err)
		return err;

	err = vtd_map_memory_region(cell, mem);
	if (err)
		vmx_unmap_memory_region(cell, mem);
	return err;
}

int arch_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	int err;

	err = vtd_unmap_memory_region(cell, mem);
	if (err)
		return err;

	return vmx_unmap_memory_region(cell, mem);
}

void arch_cell_destroy(struct per_cpu *cpu_data, struct cell *cell)
{
	ioapic_cell_exit(cell);
	pci_cell_exit(cell);
	vtd_cell_exit(cell);
	vmx_cell_exit(cell);
}

/* all root cell CPUs (except cpu_data) have to be stopped */
void arch_config_commit(struct per_cpu *cpu_data,
			struct cell *cell_added_removed)
{
	unsigned int cpu;

	for_each_cpu_except(cpu, root_cell.cpu_set, cpu_data->cpu_id)
		per_cpu(cpu)->flush_virt_caches = true;

	if (cell_added_removed && cell_added_removed != &root_cell)
		for_each_cpu_except(cpu, cell_added_removed->cpu_set,
				    cpu_data->cpu_id)
			per_cpu(cpu)->flush_virt_caches = true;

	vmx_invept();

	vtd_config_commit(cell_added_removed);
	pci_config_commit(cell_added_removed);
	ioapic_config_commit(cell_added_removed);
}

void arch_shutdown(void)
{
	pci_prepare_handover();
	ioapic_prepare_handover();

	vtd_shutdown();
	pci_shutdown();
	ioapic_shutdown();
}

void arch_suspend_cpu(unsigned int cpu_id)
{
	struct per_cpu *target_data = per_cpu(cpu_id);
	bool target_stopped;

	spin_lock(&target_data->control_lock);

	target_data->stop_cpu = true;
	target_stopped = target_data->cpu_stopped;

	spin_unlock(&target_data->control_lock);

	if (!target_stopped) {
		apic_send_nmi_ipi(target_data);

		while (!target_data->cpu_stopped)
			cpu_relax();
	}
}

void arch_resume_cpu(unsigned int cpu_id)
{
	/* make any state changes visible before releasing the CPU */
	memory_barrier();

	per_cpu(cpu_id)->stop_cpu = false;
}

/* target cpu has to be stopped */
void arch_reset_cpu(unsigned int cpu_id)
{
	per_cpu(cpu_id)->sipi_vector = APIC_BSP_PSEUDO_SIPI;

	arch_resume_cpu(cpu_id);
}

/* target cpu has to be stopped */
void arch_park_cpu(unsigned int cpu_id)
{
	per_cpu(cpu_id)->init_signaled = true;

	arch_resume_cpu(cpu_id);
}

void arch_shutdown_cpu(unsigned int cpu_id)
{
	arch_suspend_cpu(cpu_id);
	per_cpu(cpu_id)->shutdown_cpu = true;
	arch_resume_cpu(cpu_id);
	/*
	 * Note: The caller has to ensure that the target CPU has enough time
	 * to reach the shutdown position before destroying the code path it
	 * has to take to get there. This can be ensured by bringing the CPU
	 * online again under Linux before cleaning up the hypervisor.
	 */
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
		apic_send_nmi_ipi(target_data);
}

/* control_lock has to be held */
static void x86_enter_wait_for_sipi(struct per_cpu *cpu_data)
{
	cpu_data->init_signaled = false;
	cpu_data->wait_for_sipi = true;
}

int x86_handle_events(struct per_cpu *cpu_data)
{
	int sipi_vector = -1;

	spin_lock(&cpu_data->control_lock);

	do {
		if (cpu_data->init_signaled && !cpu_data->stop_cpu) {
			x86_enter_wait_for_sipi(cpu_data);
			sipi_vector = -1;
			break;
		}

		cpu_data->cpu_stopped = true;

		spin_unlock(&cpu_data->control_lock);

		while (cpu_data->stop_cpu)
			cpu_relax();

		if (cpu_data->shutdown_cpu) {
			apic_clear(cpu_data);
			vmx_cpu_exit(cpu_data);
			asm volatile("1: hlt; jmp 1b");
		}

		spin_lock(&cpu_data->control_lock);

		cpu_data->cpu_stopped = false;

		if (cpu_data->sipi_vector >= 0) {
			if (!cpu_data->failed) {
				cpu_data->wait_for_sipi = false;
				sipi_vector = cpu_data->sipi_vector;
			}
			cpu_data->sipi_vector = -1;
		}
	} while (cpu_data->init_signaled);

	if (cpu_data->flush_virt_caches) {
		cpu_data->flush_virt_caches = false;
		vmx_invept();
	}

	spin_unlock(&cpu_data->control_lock);

	/* wait_for_sipi is only modified on this CPU, so checking outside of
	 * control_lock is fine */
	if (cpu_data->wait_for_sipi)
		vmx_cpu_park(cpu_data);
	else if (sipi_vector >= 0)
		apic_clear(cpu_data);

	return sipi_vector;
}

void x86_exception_handler(struct exception_frame *frame)
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

void arch_panic_stop(struct per_cpu *cpu_data)
{
	asm volatile("1: hlt; jmp 1b");
	__builtin_unreachable();
}

void arch_panic_halt(struct per_cpu *cpu_data)
{
	spin_lock(&cpu_data->control_lock);
	x86_enter_wait_for_sipi(cpu_data);
	spin_unlock(&cpu_data->control_lock);

	vmx_cpu_park(cpu_data);
}
