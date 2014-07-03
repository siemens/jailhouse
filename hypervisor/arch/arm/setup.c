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
#include <asm/platform.h>
#include <asm/setup.h>
#include <asm/sysregs.h>
#include <jailhouse/control.h>
#include <jailhouse/entry.h>
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

	err = arch_mmu_cell_init(&root_cell);
	if (err)
		return err;

	err = arch_map_device(UART_BASE_PHYS, UART_BASE_VIRT, PAGE_SIZE);

	return err;
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	int err = 0;
	unsigned long hcr = HCR_VM_BIT | HCR_IMO_BIT | HCR_FMO_BIT
			  | HCR_TSC_BIT;

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

	err = arch_spin_init();
	if (err)
		return err;

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
	/* Setup the SPI bitmap */
	irqchip_cell_init(&root_cell);

	return map_root_memory_regions();
}

void arch_cpu_activate_vmm(struct per_cpu *cpu_data)
{
	/* Return to the kernel */
	cpu_return_el1(cpu_data);

	while (1);
}

void arch_cpu_restore(struct per_cpu *cpu_data)
{
}

// catch missing symbols
void arch_shutdown_cpu(unsigned int cpu_id) {}
void arch_shutdown(void) {}
