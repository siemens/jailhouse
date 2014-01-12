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

#include <asm/percpu.h>

struct exception_frame;

void __attribute__((noreturn))
exception_handler(struct exception_frame *frame);

void __attribute__((noreturn)) panic_stop(struct per_cpu *cpu_data);
void panic_halt(struct per_cpu *cpu_data);
