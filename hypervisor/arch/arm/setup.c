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

#include <asm/control.h>
#include <asm/irqchip.h>
#include <asm/percpu.h>
#include <asm/setup.h>
#include <asm/sysregs.h>
#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/string.h>

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
	int err = 0;

	if ((err = arch_check_features()) != 0)
		return err;

	return arch_mmu_cell_init(&root_cell);
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	int err = 0;
	unsigned long hcr = HCR_VM_BIT | HCR_IMO_BIT | HCR_FMO_BIT
			  | HCR_TSC_BIT | HCR_TAC_BIT;

	cpu_data->psci_mbox.entry = 0;
	cpu_data->virt_id = cpu_data->cpu_id;

	/*
	 * Copy the registers to restore from the linux stack here, because we
	 * won't be able to access it later
	 */
	memcpy(&cpu_data->linux_reg, (void *)cpu_data->linux_sp, NUM_ENTRY_REGS
			* sizeof(unsigned long));

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
	arm_write_sysreg(HCR, hcr);

	err = arch_mmu_cpu_cell_init(cpu_data);
	if (err)
		return err;

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
	irqchip_cell_init(&root_cell);

	/* Platform-specific SMP operations */
	register_smp_ops(&root_cell);

	err = root_cell.arch.smp->init(&root_cell);
	if (err)
		return err;

	return map_root_memory_regions();
}

void arch_cpu_activate_vmm(struct per_cpu *cpu_data)
{
	/* Return to the kernel */
	cpu_return_el1(cpu_data, false);

	while (1);
}

void arch_shutdown_self(struct per_cpu *cpu_data)
{
	irqchip_cpu_shutdown(cpu_data);

	/* Free the guest */
	arm_write_sysreg(HCR, 0);
	arm_write_sysreg(TPIDR_EL2, 0);
	arm_write_sysreg(VTCR_EL2, 0);

	/* Remove stage-2 mappings */
	arch_cpu_tlb_flush(cpu_data);

	/* TLB flush needs the cell's VMID */
	isb();
	arm_write_sysreg(VTTBR_EL2, 0);

	/* Return to EL1 */
	arch_shutdown_mmu(cpu_data);
}

void arch_cpu_restore(struct per_cpu *cpu_data, int return_code)
{
	/*
	 * If we haven't reached switch_exception_level yet, there is nothing to
	 * clean up.
	 */
	if (!is_el2())
		return;

	/*
	 * Otherwise, attempt do disable the MMU and return to EL1 using the
	 * arch_shutdown path. cpu_return will fill the banked registers and the
	 * guest regs structure (stored at the begginning of the stack) to
	 * prepare the ERET.
	 */
	cpu_return_el1(cpu_data, true);

	arch_shutdown_self(cpu_data);
}
