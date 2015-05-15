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

#ifdef CONFIG_UART_OXPCIE952
#define UART_BASE		0xe000
#else
#define UART_BASE		0x2f8
#endif

#define IPI_VECTOR		40

static volatile bool done;
static unsigned int main_cpu;

static void ipi_handler(void)
{
	printk("Received IPI on %d\n", cpu_id());
	done = true;
}

static void secondary_main(void)
{
	printk("Hello from CPU %d!\n", cpu_id());
	int_send_ipi(main_cpu, IPI_VECTOR);
}

void inmate_main(void)
{
	unsigned int n;

	printk_uart_base = UART_BASE;

	main_cpu = cpu_id();
	printk("SMP demo, primary CPU: %d\n", main_cpu);

	printk("Waiting for the rest...");
	smp_wait_for_all_cpus();
	printk("\nFound %d other CPU(s)\n", smp_num_cpus - 1);

	int_init();
	int_set_handler(IPI_VECTOR, ipi_handler);

	asm volatile("sti");

	for (n = 1; n < smp_num_cpus; n++) {
		printk(" Starting CPU %d\n", smp_cpu_ids[n]);
		done = false;
		smp_start_cpu(smp_cpu_ids[n], secondary_main);
		while (!done)
			cpu_relax();
	}
}
