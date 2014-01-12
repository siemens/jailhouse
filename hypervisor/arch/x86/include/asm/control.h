/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/percpu.h>

enum x86_init_sipi { X86_INIT, X86_SIPI };

void x86_send_init_sipi(unsigned int cpu_id, enum x86_init_sipi type,
		        int sipi_vector);

void x86_enter_wait_for_sipi(struct per_cpu *cpu_data);
int x86_handle_events(struct per_cpu *cpu_data);
