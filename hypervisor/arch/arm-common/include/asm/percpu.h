/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/irqchip.h>
#include <asm/percpu_fields.h>

#define STACK_SIZE			PAGE_SIZE

#define ARCH_PUBLIC_PERCPU_FIELDS					\
	unsigned long mpidr;						\
									\
	union {								\
		/** Only GICv2: per-cpu initialization completed. */	\
		bool gicc_initialized;					\
		/** Only GICv3: Redistributor parameters. */		\
		struct {						\
			/** Mapped redistributor base. When non-NULL,	\
			 *  per-cpu initialization completed. */	\
			void *base;					\
			/** Physical redistributor address. */		\
			unsigned long phys_addr;			\
		} gicr;							\
	};								\
									\
	struct pending_irqs pending_irqs;				\
									\
	unsigned long cpu_on_entry;					\
	unsigned long cpu_on_context;

#define ARCH_PERCPU_FIELDS						\
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
	ARM_PERCPU_FIELDS;
