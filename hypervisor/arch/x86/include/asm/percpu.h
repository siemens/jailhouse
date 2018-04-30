/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_PERCPU_H
#define _JAILHOUSE_ASM_PERCPU_H

#include <jailhouse/cell.h>
#include <asm/svm.h>
#include <asm/vmx.h>

#define NUM_ENTRY_REGS			6

#define STACK_SIZE			PAGE_SIZE

/* Round up sizeof(struct per_cpu) to the next power of two. */
#define PERCPU_SIZE_SHIFT \
	(BITS_PER_LONG - __builtin_clzl(sizeof(struct per_cpu) - 1))

/**
 * @defgroup Per-CPU Per-CPU Subsystem
 *
 * The per-CPU subsystem provides a CPU-local state structure and accessors.
 *
 * @{
 */

/** Per-CPU states. */
struct per_cpu {
	union {
		/** Stack used while in hypervisor mode. */
		u8 stack[STACK_SIZE];
		struct {
			u8 __fill[STACK_SIZE - sizeof(union registers)];
			/** Guest registers saved on stack during VM exit. */
			union registers guest_regs;
		};
	};

	/** Linux stack pointer, used for handover to hypervisor. */
	unsigned long linux_sp;

	/** Self reference, required for this_cpu_data(). */
	struct per_cpu *cpu_data;
	/** Logical CPU ID (same as Linux). */
	unsigned int cpu_id;
	/** Physical APIC ID. */
	u32 apic_id;
	/** Owning cell. */
	struct cell *cell;

	/** Statistic counters. */
	u32 stats[JAILHOUSE_NUM_CPU_STATS];

	/** Linux states, used for handover to/from hypervisor. @{ */
	struct desc_table_reg linux_gdtr;
	struct desc_table_reg linux_idtr;
	unsigned long linux_reg[NUM_ENTRY_REGS];
	unsigned long linux_ip;
	unsigned long linux_cr0;
	unsigned long linux_cr3;
	unsigned long linux_cr4;
	struct segment linux_cs;
	struct segment linux_ds;
	struct segment linux_es;
	struct segment linux_fs;
	struct segment linux_gs;
	struct segment linux_tss;
	unsigned long linux_efer;
	/** @} */

	/** Shadow states. @{ */
	unsigned long pat;
	unsigned long mtrr_def_type;
	/** @} */

	/** Cached PDPTEs, used by VMX for PAE guest paging mode. */
	unsigned long pdpte[4];

	/** True when CPU is initialized by hypervisor. */
	bool initialized;
	union {
		/** VMX initialization state */
		enum vmx_state vmx_state;
		/** SVM initialization state */
		enum {SVMOFF = 0, SVMON} svm_state;
	};

	/**
	 * Lock protecting CPU state changes done for control tasks.
	 *
	 * The lock protects the following fields (unless CPU is suspended):
	 * @li per_cpu::suspend_cpu
	 * @li per_cpu::cpu_suspended (except for spinning on it to become
	 *                             true)
	 * @li per_cpu::wait_for_sipi
	 * @li per_cpu::init_signaled
	 * @li per_cpu::sipi_vector
	 * @li per_cpu::flush_vcpu_caches
	 */
	spinlock_t control_lock;

	/** Set to true for instructing the CPU to suspend. */
	volatile bool suspend_cpu;
	/** True if CPU is waiting for SIPI. */
	volatile bool wait_for_sipi;
	/** True if CPU is suspended. */
	volatile bool cpu_suspended;
	/** Set to true for pending an INIT signal. */
	bool init_signaled;
	/** Pending SIPI vector; -1 if none is pending. */
	int sipi_vector;
	/** Set to true for a pending TLB flush for the paging layer that does
	 *  host physical <-> guest physical memory mappings. */
	bool flush_vcpu_caches;
	/** Set to true for pending cache allocation updates (Intel only). */
	bool update_cat;
	/** State of the shutdown process. Possible values:
	 * @li SHUTDOWN_NONE: no shutdown in progress
	 * @li SHUTDOWN_STARTED: shutdown in progress
	 * @li negative error code: shutdown failed
	 */
	int shutdown_state;
	/** True if CPU violated a cell boundary or cause some other failure in
	 * guest mode. */
	bool failed;

	/** Number of iterations to clear pending APIC IRQs. */
	unsigned int num_clear_apic_irqs;

	union {
		struct {
			/** VMXON region, required by VMX. */
			struct vmcs vmxon_region
				__attribute__((aligned(PAGE_SIZE)));
			/** VMCS of this CPU, required by VMX. */
			struct vmcs vmcs
				__attribute__((aligned(PAGE_SIZE)));
		};
		struct {
			/** VMCB block, required by SVM. */
			struct vmcb vmcb
				__attribute__((aligned(PAGE_SIZE)));
			/** SVM Host save area; opaque to us. */
			u8 host_state[PAGE_SIZE]
				__attribute__((aligned(PAGE_SIZE)));
		};
	};
} __attribute__((aligned(PAGE_SIZE)));

/**
 * Define CPU-local accessor for a per-CPU field.
 * @param field		Field name.
 *
 * The accessor will have the form of a function, returning the correspondingly
 * typed field value: @c this_field().
 */
#define DEFINE_PER_CPU_ACCESSOR(field)					    \
static inline typeof(((struct per_cpu *)0)->field) this_##field(void)	    \
{									    \
	typeof(((struct per_cpu *)0)->field) tmp;			    \
									    \
	asm volatile(							    \
		"mov %%gs:%1,%0\n\t"					    \
		: "=&q" (tmp)						    \
		: "m" (*(u8 *)__builtin_offsetof(struct per_cpu, field)));  \
	return tmp;							    \
}

/**
 * Retrieve the data structure of the current CPU.
 *
 * @return Pointer to per-CPU data structure.
 */
static inline struct per_cpu *this_cpu_data(void);
DEFINE_PER_CPU_ACCESSOR(cpu_data)

/**
 * Retrieve the ID of the current CPU.
 *
 * @return CPU ID.
 */
static inline unsigned int this_cpu_id(void);
DEFINE_PER_CPU_ACCESSOR(cpu_id)

/**
 * Retrieve the cell owning the current CPU.
 *
 * @return Pointer to cell.
 */
static inline struct cell *this_cell(void);
DEFINE_PER_CPU_ACCESSOR(cell)

/**
 * Retrieve the data structure of the specified CPU.
 * @param cpu	ID of the target CPU.
 *
 * @return Pointer to per-CPU data structure.
 */
static inline struct per_cpu *per_cpu(unsigned int cpu)
{
	struct per_cpu *cpu_data;

	asm volatile(
		"lea __page_pool(%%rip),%0\n\t"
		"add %1,%0\n\t"
		: "=&q" (cpu_data)
		: "qm" ((unsigned long)cpu << PERCPU_SIZE_SHIFT));
	return cpu_data;
}

/** @} **/

#endif /* !_JAILHOUSE_ASM_PERCPU_H */
