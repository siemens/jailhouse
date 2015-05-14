/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

#define APIC_DM_INIT	(5 << 8)
#define APIC_DM_SIPI	(6 << 8)

extern void (* volatile ap_entry)(void);

void smp_wait_for_all_cpus(void)
{
	while (smp_num_cpus < comm_region->num_cpus)
		cpu_relax();
}

void smp_start_cpu(unsigned int cpu_id, void (*entry)(void))
{
	u64 base_val = ((u64)cpu_id << 32) | APIC_LVL_ASSERT;

	ap_entry = entry;

	write_msr(X2APIC_ICR, base_val | APIC_DM_INIT);
	delay_us(10000);
	write_msr(X2APIC_ICR, base_val | APIC_DM_SIPI | 0xf0);
	delay_us(200);
	write_msr(X2APIC_ICR, base_val | APIC_DM_SIPI | 0xf0);

	while (ap_entry != NULL)
		cpu_relax();
}
