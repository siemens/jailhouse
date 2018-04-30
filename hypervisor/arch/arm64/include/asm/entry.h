/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/percpu.h>

void enable_mmu_el2(u64 ttbr0_el2);
void __attribute__((noreturn)) shutdown_el2(struct per_cpu *cpu_data);

void __attribute__((noreturn)) vmreturn(struct registers *guest_regs);
