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

#ifndef _JAILHOUSE_ASM_PERCPU_H
#define _JAILHOUSE_ASM_PERCPU_H

#include <jailhouse/types.h>

#ifndef __ASSEMBLY__

#include <jailhouse/cell.h>
#include <asm/irqchip.h>
#include <asm/spinlock.h>

/* Round up sizeof(struct per_cpu) to the next power of two. */
#define PERCPU_SIZE_SHIFT \
        (BITS_PER_LONG - __builtin_clzl(sizeof(struct per_cpu) - 1))

struct per_cpu {
	u8 stack[PAGE_SIZE];
	unsigned long saved_vectors;

	/* common fields */
	unsigned int cpu_id;
	struct cell *cell;
	u32 stats[JAILHOUSE_NUM_CPU_STATS];
	int shutdown_state;
	bool failed;

	/* synchronizes parallel insertions of SGIs into the pending ring */
	spinlock_t pending_irqs_lock;
	u16 pending_irqs[MAX_PENDING_IRQS];
	unsigned int pending_irqs_head;
	/* removal from the ring happens lockless, thus tail is volatile */
	volatile unsigned int pending_irqs_tail;

	union {
		/** Only GICv2: per-cpu initialization completed. */
		bool gicc_initialized;
		/** Only GICv3: physical redistributor base. When non-NULL,
		 * per-cpu initialization completed. */
		void *gicr_base;
	};

	unsigned long mpidr;

	/**
	 * Lock protecting CPU state changes done for control tasks.
	 *
	 * The lock protects the following fields (unless CPU is suspended):
	 * @li per_cpu::suspend_cpu
	 * @li per_cpu::cpu_suspended (except for spinning on it to become
	 *                             true)
	 * @li per_cpu::flush_vcpu_caches
	 */
	spinlock_t control_lock;

	/** Set to true for instructing the CPU to suspend. */
	volatile bool suspend_cpu;
	/** True if CPU is waiting for power-on. */
	volatile bool wait_for_poweron;
	/** True if CPU is suspended. */
	volatile bool cpu_suspended;
	/** Set to true for pending reset. */
	bool reset;
	/** Set to true for pending park. */
	bool park;
	/** Set to true for a pending TLB flush for the paging layer that does
	 *  host physical <-> guest physical memory mappings. */
	bool flush_vcpu_caches;

	unsigned long cpu_on_entry;
	unsigned long cpu_on_context;
} __attribute__((aligned(PAGE_SIZE)));

static inline struct per_cpu *this_cpu_data(void)
{
	struct per_cpu *cpu_data;

	arm_read_sysreg(TPIDR_EL2, cpu_data);
	return cpu_data;
}

#define DEFINE_PER_CPU_ACCESSOR(field)					\
static inline typeof(((struct per_cpu *)0)->field) this_##field(void)	\
{									\
	return this_cpu_data()->field;					\
}

DEFINE_PER_CPU_ACCESSOR(cpu_id)
DEFINE_PER_CPU_ACCESSOR(cell)

static inline struct per_cpu *per_cpu(unsigned int cpu)
{
	extern u8 __page_pool[];

	return (struct per_cpu *)(__page_pool + (cpu << PERCPU_SIZE_SHIFT));
}

static inline struct registers *guest_regs(struct per_cpu *cpu_data)
{
	/* assumes that the cell registers are at the beginning of the stack */
	return (struct registers *)(cpu_data->stack + sizeof(cpu_data->stack)
			- sizeof(struct registers));
}

/*
 * We get rid of the virt_id in the AArch64 implementation, since it
 * doesn't really fit with the MPIDR CPU identification scheme on ARM.
 *
 * Until the GICv3 and ARMv7 code has been properly refactored to
 * support this scheme, we stub this call so we can share the GICv2
 * code with ARMv7.
 *
 * TODO: implement MPIDR support in the GICv3 code, so it can be
 * used on AArch64.
 * TODO: refactor out virt_id from the AArch7 port as well.
 */
unsigned int arm_cpu_phys2virt(unsigned int cpu_id);
#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PERCPU_H */
