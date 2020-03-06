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

#include <jailhouse/percpu.h>

struct exception_frame;

enum x86_init_sipi { X86_INIT, X86_SIPI };

void x86_send_init_sipi(unsigned int cpu_id, enum x86_init_sipi type,
			int sipi_vector);

void x86_check_events(void);

void __attribute__((noreturn))
x86_exception_handler(struct exception_frame *frame);
