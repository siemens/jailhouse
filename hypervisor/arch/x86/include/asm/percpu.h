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

#include <jailhouse/cell.h>
#include <asm/svm.h>
#include <asm/vmx.h>

#define NUM_ENTRY_REGS			6

#define STACK_SIZE			PAGE_SIZE

#define ARCH_PERCPU_FIELDS						\
	/** Linux stack pointer, used for handover to hypervisor. */	\
	unsigned long linux_sp;						\
									\
	/** Physical APIC ID. */					\
	u32 apic_id;							\
									\
	/** Linux states, used for handover to/from hypervisor. @{ */	\
	struct desc_table_reg linux_gdtr;				\
	struct desc_table_reg linux_idtr;				\
	unsigned long linux_reg[NUM_ENTRY_REGS];			\
	unsigned long linux_ip;						\
	unsigned long linux_cr0;					\
	unsigned long linux_cr3;					\
	unsigned long linux_cr4;					\
	struct segment linux_cs;					\
	struct segment linux_ds;					\
	struct segment linux_es;					\
	struct segment linux_fs;					\
	struct segment linux_gs;					\
	struct segment linux_tss;					\
	unsigned long linux_efer;					\
	/** @} */							\
									\
	/** Shadow states. @{ */					\
	unsigned long pat;						\
	unsigned long mtrr_def_type;					\
	/** @} */							\
									\
	/** Cached PDPTEs, used by VMX for PAE guest paging mode. */	\
	unsigned long pdpte[4];						\
									\
	/* IOMMU request completion flags */				\
	union {								\
		volatile u32 vtd_iq_completed;				\
		volatile u64 amd_iommu_sem;				\
	};								\
									\
	/** True when CPU is initialized by hypervisor. */		\
	bool initialized;						\
	union {								\
		/** VMX initialization state */				\
		enum vmx_state vmx_state;				\
		/** SVM initialization state */				\
		enum {SVMOFF = 0, SVMON} svm_state;			\
	};								\
									\
	/**								\
	 * Lock protecting CPU state changes done for control tasks.	\
	 *								\
	 * The lock protects the following fields (unless CPU is	\
	 * suspended):							\
	 * @li per_cpu::suspend_cpu					\
	 * @li per_cpu::cpu_suspended (except for spinning on it to	\
	 *                             become true)			\
	 * @li per_cpu::wait_for_sipi					\
	 * @li per_cpu::init_signaled					\
	 * @li per_cpu::sipi_vector					\
	 * @li per_cpu::flush_vcpu_caches				\
	 */								\
	spinlock_t control_lock;					\
									\
	/** True if CPU is waiting for SIPI. */				\
	volatile bool wait_for_sipi;					\
	/** Set to true for pending an INIT signal. */			\
	bool init_signaled;						\
	/** Pending SIPI vector; -1 if none is pending. */		\
	int sipi_vector;						\
	/** Set to true for pending cache allocation updates (Intel	\
	 *  only). */							\
	bool update_cat;						\
									\
	/** Number of iterations to clear pending APIC IRQs. */		\
	unsigned int num_clear_apic_irqs;				\
									\
	union {								\
		struct {						\
			/** VMXON region, required by VMX. */		\
			struct vmcs vmxon_region			\
				__attribute__((aligned(PAGE_SIZE)));	\
			/** VMCS of this CPU, required by VMX. */	\
			struct vmcs vmcs				\
				__attribute__((aligned(PAGE_SIZE)));	\
		};							\
		struct {						\
			/** VMCB block, required by SVM. */		\
			struct vmcb vmcb				\
				__attribute__((aligned(PAGE_SIZE)));	\
			/** SVM Host save area; opaque to us. */	\
			u8 host_state[PAGE_SIZE]			\
				__attribute__((aligned(PAGE_SIZE)));	\
		};							\
	};
