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

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <asm/control.h>
#include <asm/irqchip.h>
#include <asm/psci.h>
#include <asm/traps.h>

void arm_cpu_reset(unsigned long pc, bool aarch32)
{
	u64 hcr_el2;
	u64 fpexc32_el2;

	/* put the cpu in a reset state */
	/* AARCH64_TODO: handle big endian support */
	arm_write_sysreg(SCTLR_EL1, SCTLR_EL1_RES1);
	arm_write_sysreg(CNTKCTL_EL1, 0);
	arm_write_sysreg(PMCR_EL0, 0);

	/* wipe any other state to avoid leaking information accross cells */
	memset(&this_cpu_data()->guest_regs, 0, sizeof(union registers));

	/* AARCH64_TODO: wipe floating point registers */

	/* wipe special registers */
	arm_write_sysreg(SP_EL0, 0);
	arm_write_sysreg(SP_EL1, 0);
	arm_write_sysreg(SPSR_EL1, 0);

	/* wipe the system registers */
	arm_write_sysreg(AFSR0_EL1, 0);
	arm_write_sysreg(AFSR1_EL1, 0);
	arm_write_sysreg(AMAIR_EL1, 0);
	arm_write_sysreg(CONTEXTIDR_EL1, 0);
	arm_write_sysreg(CPACR_EL1, CPACR_EL1_FPEN_ALL);
	arm_write_sysreg(CSSELR_EL1, 0);
	arm_write_sysreg(ESR_EL1, 0);
	arm_write_sysreg(FAR_EL1, 0);
	arm_write_sysreg(MAIR_EL1, 0);
	arm_write_sysreg(PAR_EL1, 0);
	arm_write_sysreg(TCR_EL1, 0);
	arm_write_sysreg(TPIDRRO_EL0, 0);
	arm_write_sysreg(TPIDR_EL0, 0);
	arm_write_sysreg(TPIDR_EL1, 0);
	arm_write_sysreg(TTBR0_EL1, 0);
	arm_write_sysreg(TTBR1_EL1, 0);
	arm_write_sysreg(VBAR_EL1, 0);

	arm_read_sysreg(FPEXC32_EL2, fpexc32_el2);
	fpexc32_el2 |= FPEXC_EL2_EN_BIT;
	arm_write_sysreg(FPEXC32_EL2, fpexc32_el2);

	/* wipe timer registers */
	arm_write_sysreg(CNTP_CTL_EL0, 0);
	arm_write_sysreg(CNTP_CVAL_EL0, 0);
	arm_write_sysreg(CNTP_TVAL_EL0, 0);
	arm_write_sysreg(CNTV_CTL_EL0, 0);
	arm_write_sysreg(CNTV_CVAL_EL0, 0);
	arm_write_sysreg(CNTV_TVAL_EL0, 0);

	/* AARCH64_TODO: handle PMU registers */
	/* AARCH64_TODO: handle debug registers */
	/* AARCH64_TODO: handle system registers for AArch32 state */
	arm_read_sysreg(HCR_EL2, hcr_el2);
	if (aarch32) {
		arm_write_sysreg(SPSR_EL2, RESET_PSR_AARCH32);
		hcr_el2 &= ~HCR_RW_BIT;
	} else {
		arm_write_sysreg(SPSR_EL2, RESET_PSR_AARCH64);
		hcr_el2 |= HCR_RW_BIT;
	}
	arm_write_sysreg(HCR_EL2, hcr_el2);

	arm_write_sysreg(ELR_EL2, pc);

	/* transfer the context that may have been passed to PSCI_CPU_ON */
	this_cpu_data()->guest_regs.usr[1] = this_cpu_public()->cpu_on_context;

	arm_paging_vcpu_init(&this_cell()->arch.mm);

	irqchip_cpu_reset(this_cpu_data());
}

#ifdef CONFIG_CRASH_CELL_ON_PANIC
void arch_panic_park(void)
{
	arm_write_sysreg(ELR_EL2, 0);
}
#endif

void arm_cpu_passthru_suspend(void)
{
	unsigned long hcr;

	arm_read_sysreg(HCR_EL2, hcr);
	arm_write_sysreg(HCR_EL2, hcr | HCR_IMO_BIT | HCR_FMO_BIT);
	isb();
	asm volatile("wfi" : : : "memory");
	arm_write_sysreg(HCR_EL2, hcr);
}
