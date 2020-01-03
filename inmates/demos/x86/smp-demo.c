/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

#define IPI_VECTOR		40

static volatile bool done;
static unsigned int main_cpu;

static void ipi_handler(unsigned int irq)
{
	if (irq != IPI_VECTOR)
		return;

	printk("Received IPI on %d\n", cpu_id());
	done = true;
}

static void secondary_main(void)
{
	printk("Hello from CPU %d!\n", cpu_id());
	irq_send_ipi(main_cpu, IPI_VECTOR);
}

void inmate_main(void)
{
	unsigned int n;

	main_cpu = cpu_id();
	printk("SMP demo, primary CPU: %d\n", main_cpu);

	printk("Waiting for the rest...");
	smp_wait_for_all_cpus();
	printk("\nFound %d other CPU(s)\n", smp_num_cpus - 1);

	irq_init(ipi_handler);

	asm volatile("sti");

	for (n = 1; n < smp_num_cpus; n++) {
		printk(" Starting CPU %d\n", smp_cpu_ids[n]);
		done = false;
		smp_start_cpu(smp_cpu_ids[n], secondary_main);
		while (!done)
			cpu_relax();
	}
}
