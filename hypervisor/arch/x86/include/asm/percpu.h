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

#ifndef _JAILHOUSE_ASM_PERCPU_H
#define _JAILHOUSE_ASM_PERCPU_H

#include <asm/types.h>
#include <asm/paging.h>
#include <asm/processor.h>

#include <jailhouse/hypercall.h>

#define NUM_ENTRY_REGS			6

/* Keep in sync with struct per_cpu! */
#define PERCPU_SIZE_SHIFT		14
#define PERCPU_STACK_END		PAGE_SIZE
#define PERCPU_LINUX_SP			PERCPU_STACK_END

#ifndef __ASSEMBLY__

#include <asm/cell.h>
#include <asm/spinlock.h>

struct vmcs {
	u32 revision_id:31;
	u32 shadow_indicator:1;
	u32 abort_indicator;
	u64 data[(PAGE_SIZE - 4 - 4) / 8];
} __attribute__((packed));

struct per_cpu {
	/* Keep these two in sync with defines above! */
	u8 stack[PAGE_SIZE];
	unsigned long linux_sp;

	unsigned int cpu_id;
	u32 apic_id;
	struct cell *cell;

	u32 stats[JAILHOUSE_NUM_CPU_STATS];

	struct desc_table_reg linux_gdtr;
	struct desc_table_reg linux_idtr;
	unsigned long linux_reg[NUM_ENTRY_REGS];
	unsigned long linux_ip;
	unsigned long linux_cr3;
	struct segment linux_cs;
	struct segment linux_ds;
	struct segment linux_es;
	struct segment linux_fs;
	struct segment linux_gs;
	struct segment linux_tss;
	unsigned long linux_efer;
	unsigned long linux_sysenter_cs;
	unsigned long linux_sysenter_eip;
	unsigned long linux_sysenter_esp;
	bool initialized;
	enum { VMXOFF = 0, VMXON, VMCS_READY } vmx_state;

	/*
	 * protects the following per_cpu fields (unless CPU is stopped):
	 *  - stop_cpu
	 *  - cpu_stopped (except for spinning on it to become true)
	 *  - wait_for_sipi
	 *  - init_signaled
	 *  - sipi_vector
	 *  - flush_caches
	 */
	spinlock_t control_lock;

	volatile bool stop_cpu;
	volatile bool wait_for_sipi;
	volatile bool cpu_stopped;
	bool init_signaled;
	int sipi_vector;
	bool flush_virt_caches;
	bool shutdown_cpu;
	int shutdown_state;
	bool failed;

	struct vmcs vmxon_region __attribute__((aligned(PAGE_SIZE)));
	struct vmcs vmcs __attribute__((aligned(PAGE_SIZE)));
} __attribute__((aligned(PAGE_SIZE)));

static inline struct per_cpu *per_cpu(unsigned int cpu)
{
	struct per_cpu *cpu_data;

	asm volatile(
		"lea __page_pool(%%rip),%0\n\t"
		"add %1,%0\n\t"
		: "=&qm" (cpu_data)
		: "qm" ((unsigned long)cpu << PERCPU_SIZE_SHIFT));
	return cpu_data;
}

/* Validate defines */
#define CHECK_ASSUMPTION(assume)	((void)sizeof(char[1 - 2*!(assume)]))

static inline void __check_assumptions(void)
{
	struct per_cpu cpu_data;

	CHECK_ASSUMPTION(sizeof(struct per_cpu) == (1 << PERCPU_SIZE_SHIFT));
	CHECK_ASSUMPTION(sizeof(cpu_data.stack) == PERCPU_STACK_END);
	CHECK_ASSUMPTION(__builtin_offsetof(struct per_cpu, linux_sp) ==
			 PERCPU_LINUX_SP);
}
#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PERCPU_H */
