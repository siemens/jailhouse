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
#include <asm/mach.h>
#include <asm/setup.h>
#include <asm/sysregs.h>

static u32 __attribute__((aligned(PAGE_SIZE))) parking_code[PAGE_SIZE / 4] = {
	0xe320f003, /* 1: wfi  */
	0xeafffffd, /*    b 1b */
};

unsigned int cache_line_size;
struct paging_structures parking_mm;

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

	parking_mm.root_paging = cell_paging;
	parking_mm.root_table =
		page_alloc_aligned(&mem_pool, ARM_CELL_ROOT_PT_SZ);
	if (!parking_mm.root_table)
		return -ENOMEM;

	err = paging_create(&parking_mm, paging_hvirt2phys(parking_code),
			    PAGE_SIZE, 0,
			    (PTE_FLAG_VALID | PTE_ACCESS_FLAG |
			     S2_PTE_ACCESS_RO | S2_PTE_FLAG_NORMAL),
			    PAGING_COHERENT);
	if (err)
		return err;

	return arm_paging_cell_init(&root_cell);
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	int err;

	cpu_data->virt_id = cpu_data->cpu_id;
	cpu_data->mpidr = phys_processor_id();

	/*
	 * Copy the registers to restore from the linux stack here, because we
	 * won't be able to access it later
	 */
	memcpy(&cpu_data->linux_reg, (void *)cpu_data->linux_sp,
	       NUM_ENTRY_REGS * sizeof(unsigned long));

	err = switch_exception_level(cpu_data);
	if (err)
		return err;

	/*
	 * Save pointer in the thread local storage
	 * Must be done early in order to handle aborts and errors in the setup
	 * code.
	 */
	arm_write_sysreg(TPIDR_EL2, cpu_data);

	/* Setup guest traps */
	arm_write_sysreg(HCR, HCR_VM_BIT | HCR_IMO_BIT | HCR_FMO_BIT |
			      HCR_TSC_BIT | HCR_TAC_BIT | HCR_TSW_BIT);

	arm_paging_vcpu_init(&root_cell.arch.mm);

	err = irqchip_init();
	if (err)
		return err;

	err = irqchip_cpu_init(cpu_data);

	return err;
}

int arch_init_late(void)
{
	int err;

	/* Setup the SPI bitmap */
	err = irqchip_cell_init(&root_cell);
	if (err)
		return err;

	err = mach_init();
	if (err)
		return err;

	return map_root_memory_regions();
}

void __attribute__((noreturn)) arch_cpu_activate_vmm(struct per_cpu *cpu_data)
{
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
	irqchip_cpu_shutdown(cpu_data);

	/* Free the guest */
	arm_write_sysreg(HCR, 0);
	arm_write_sysreg(TPIDR_EL2, 0);
	arm_write_sysreg(VTCR_EL2, 0);

	/* Remove stage-2 mappings */
	arm_paging_vcpu_flush_tlbs();

	/* TLB flush needs the cell's VMID */
	isb();
	arm_write_sysreg(VTTBR_EL2, 0);

	/* Return to EL1 */
	arch_shutdown_mmu(cpu_data);
}

void arch_cpu_restore(struct per_cpu *cpu_data, int return_code)
{
	struct registers *ctx = guest_regs(cpu_data);

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

	memcpy(&ctx->usr, &cpu_data->linux_reg,
	       NUM_ENTRY_REGS * sizeof(unsigned long));

	arch_shutdown_self(cpu_data);
}
