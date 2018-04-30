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

#include <jailhouse/cell.h>
#include <asm/irqchip.h>

#define NUM_ENTRY_REGS			13

#define STACK_SIZE			PAGE_SIZE

#define ARCH_PERCPU_FIELDS						\
	/** Linux stack pointer, used for handover to the hypervisor. */ \
	unsigned long linux_sp;						\
	unsigned long linux_ret;					\
	unsigned long linux_flags;					\
	unsigned long linux_reg[NUM_ENTRY_REGS];			\
									\
	struct pending_irqs pending_irqs;				\
									\
	union {								\
		/** Only GICv2: per-cpu initialization completed. */	\
		bool gicc_initialized;					\
		/** Only GICv3 */					\
		struct {						\
			/** mapped redistributor base. When non-NULL,	\
			 *  per-cpu initialization completed. */	\
			void *base;					\
			/** physical redistributor address */		\
			unsigned long phys_addr;			\
		} gicr;							\
	};								\
									\
	bool initialized;						\
									\
	/**								\
	 * Lock protecting CPU state changes done for control tasks.	\
	 *								\
	 * The lock protects the following fields (unless CPU is	\
	 * suspended):							\
	 * @li per_cpu::suspend_cpu					\
	 * @li per_cpu::cpu_suspended (except for spinning on it to	\
	 *                             become true)			\
	 * @li per_cpu::flush_vcpu_caches				\
	 */								\
	spinlock_t control_lock;					\
									\
	/** True if CPU is waiting for power-on. */			\
	volatile bool wait_for_poweron;					\
	/** Set to true for pending reset. */				\
	bool reset;							\
	/** Set to true for pending park. */				\
	bool park;							\
									\
	unsigned long cpu_on_entry;					\
	unsigned long cpu_on_context;					\
									\
	unsigned long mpidr;
