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
#include <asm/apic.h>
#include <asm/control.h>
#include <asm/spinlock.h>
#include <asm/vmx.h>
#include <asm/vtd.h>

static DEFINE_SPINLOCK(wait_lock);

static void flush_linux_cpu_caches(struct per_cpu *cpu_data)
{
	unsigned int cpu;

	for_each_cpu_except(cpu, linux_cell.cpu_set, cpu_data->cpu_id)
		per_cpu(cpu)->flush_caches = true;
}

int arch_cell_create(struct per_cpu *cpu_data, struct cell *cell)
{
	int err;

	/* TODO: Implement proper roll-backs on errors */

	vmx_linux_cell_shrink(cell->config);
	flush_linux_cpu_caches(cpu_data);
	err = vmx_cell_init(cell);
	if (err)
		return err;

	vtd_linux_cell_shrink(cell->config);
	err = vtd_cell_init(cell);
	if (err)
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

void arch_unmap_memory_region(struct cell *cell,
			      const struct jailhouse_memory *mem)
{
	vtd_unmap_memory_region(cell, mem);
	vmx_unmap_memory_region(cell, mem);
}

void arch_cell_destroy(struct per_cpu *cpu_data, struct cell *cell)
{
	vtd_cell_exit(cell);
	vmx_cell_exit(cell);
	flush_linux_cpu_caches(cpu_data);
}

void arch_shutdown(void)
{
	vtd_shutdown();
}

void arch_suspend_cpu(unsigned int cpu_id)
{
	struct per_cpu *target_data = per_cpu(cpu_id);
	bool target_stopped;

	spin_lock(&wait_lock);

	target_data->stop_cpu = true;
	target_stopped = target_data->cpu_stopped;

	spin_unlock(&wait_lock);

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
	per_cpu(cpu_id)->wait_for_sipi = true;
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

	spin_lock(&wait_lock);

	if (type == X86_INIT) {
		if (!target_data->wait_for_sipi) {
			target_data->init_signaled = true;
			send_nmi = true;
		}
	} else if (target_data->wait_for_sipi) {
		target_data->sipi_vector = sipi_vector;
		send_nmi = true;
	}

	spin_unlock(&wait_lock);

	if (send_nmi)
		apic_send_nmi_ipi(target_data);
}

int x86_handle_events(struct per_cpu *cpu_data)
{
	int sipi_vector = -1;

	spin_lock(&wait_lock);

	do {
		if (cpu_data->init_signaled && !cpu_data->stop_cpu) {
			cpu_data->init_signaled = false;
			cpu_data->wait_for_sipi = true;
			sipi_vector = -1;
			apic_clear();
			vmx_cpu_park();
			break;
		}

		cpu_data->cpu_stopped = true;

		spin_unlock(&wait_lock);

		while (cpu_data->stop_cpu)
			cpu_relax();

		if (cpu_data->shutdown_cpu) {
			apic_clear();
			vmx_cpu_exit(cpu_data);
			asm volatile("1: hlt; jmp 1b");
		}

		spin_lock(&wait_lock);

		cpu_data->cpu_stopped = false;

		if (cpu_data->wait_for_sipi) {
			cpu_data->wait_for_sipi = false;
			sipi_vector = cpu_data->sipi_vector;
		}
	} while (cpu_data->init_signaled);

	if (cpu_data->flush_caches) {
		cpu_data->flush_caches = false;
		flush_tlb();
		vmx_invept();
	}

	spin_unlock(&wait_lock);

	return sipi_vector;
}
