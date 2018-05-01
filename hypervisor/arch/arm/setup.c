/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/processor.h>
#include <jailhouse/string.h>
#include <asm/control.h>
#include <asm/mmu_hyp.h>
#include <asm/setup.h>
#include <asm/sysregs.h>

unsigned int cache_line_size;

static int arch_check_features(void)
{
	u32 pfr1;
	u32 ctr;

	arm_read_sysreg(ID_PFR1_EL1, pfr1);

	if (!PFR1_VIRT(pfr1))
		return -ENODEV;

	arm_read_sysreg(CTR_EL0, ctr);
	/* Extract the minimal cache line size */
	cache_line_size = 4 << (ctr >> 16 & 0xf);

	return 0;
}

int arch_init_early(void)
{
	int err;

	err = arch_check_features();
	if (err)
		return err;

	return arm_init_early();
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	int err;

	/*
	 * Copy the registers to restore from the linux stack here, because we
	 * won't be able to access it later
	 */
	memcpy(&cpu_data->linux_reg, (void *)cpu_data->linux_sp,
	       NUM_ENTRY_REGS * sizeof(unsigned long));

	err = switch_exception_level(cpu_data);
	if (err)
		return err;

	/* Setup guest traps */
	arm_write_sysreg(HCR, HCR_VM_BIT | HCR_IMO_BIT | HCR_FMO_BIT |
			      HCR_TSC_BIT | HCR_TAC_BIT | HCR_TSW_BIT);

	return arm_cpu_init(cpu_data);
}

static inline void __attribute__((always_inline))
cpu_prepare_return_el1(struct per_cpu *cpu_data, int return_code)
{
	cpu_data->linux_reg[0] = return_code;

	asm volatile (
		"msr	sp_svc, %0\n\t"
		"msr	elr_hyp, %1\n\t"
		"msr	spsr_hyp, %2\n\t"
		:
		: "r" (cpu_data->linux_sp +
		       (NUM_ENTRY_REGS * sizeof(unsigned long))),
		  "r" (cpu_data->linux_ret),
		  "r" (cpu_data->linux_flags));
}

void __attribute__((noreturn)) arch_cpu_activate_vmm(void)
{
	struct per_cpu *cpu_data = this_cpu_data();

	/* Revoke full per_cpu access now that everything is set up. */
	paging_map_all_per_cpu(this_cpu_id(), false);

	/* Return to the kernel */
	cpu_prepare_return_el1(cpu_data, 0);

	asm volatile(
		/* Reset the hypervisor stack */
		"mov	sp, %0\n\t"
		/*
		 * We don't care about clobbering the other registers from now
		 * on. Must be in sync with arch_entry.
		 */
		"ldm	%1, {r0 - r12}\n\t"
		/*
		 * After this, the kernel won't be able to access the hypervisor
		 * code.
		 */
		"eret\n\t"
		:
		: "r" (cpu_data->stack + sizeof(cpu_data->stack)),
		  "r" (cpu_data->linux_reg));

	__builtin_unreachable();
}

void arch_shutdown_self(struct per_cpu *cpu_data)
{
	irqchip_cpu_shutdown(&cpu_data->public);

	/* Free the guest */
	arm_write_sysreg(HCR, 0);
	arm_write_sysreg(VTCR_EL2, 0);

	/* Remove stage-2 mappings */
	arm_paging_vcpu_flush_tlbs();

	/* TLB flush needs the cell's VMID */
	isb();
	arm_write_sysreg(VTTBR_EL2, 0);

	/* Return to EL1 */
	arch_shutdown_mmu(cpu_data);
}

void arch_cpu_restore(unsigned int cpu_id, int return_code)
{
	struct per_cpu *cpu_data = per_cpu(cpu_id);

	/*
	 * If we haven't reached switch_exception_level yet, there is nothing to
	 * clean up.
	 */
	if (!is_el2())
		return;

	/*
	 * Otherwise, attempt do disable the MMU and return to EL1 using the
	 * arch_shutdown path. cpu_return will fill the banked registers and the
	 * guest regs structure (stored at the beginning of the stack) to
	 * prepare the ERET.
	 */
	cpu_prepare_return_el1(cpu_data, return_code);

	memcpy(&cpu_data->guest_regs.usr, &cpu_data->linux_reg,
	       NUM_ENTRY_REGS * sizeof(unsigned long));

	arch_shutdown_self(cpu_data);
}
