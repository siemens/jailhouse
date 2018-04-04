/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
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
#include <asm/sysregs.h>

void arm_cpu_reset(unsigned long pc)
{
	struct per_cpu *cpu_data = this_cpu_data();
	struct cell *cell = cpu_data->cell;
	struct registers *regs = guest_regs(cpu_data);
	u32 sctlr;

	/* Wipe all banked and usr regs */
	memset(regs, 0, sizeof(struct registers));

	arm_write_banked_reg(SP_usr, 0);
	arm_write_banked_reg(SP_svc, 0);
	arm_write_banked_reg(SP_abt, 0);
	arm_write_banked_reg(SP_und, 0);
	arm_write_banked_reg(SP_irq, 0);
	arm_write_banked_reg(SP_fiq, 0);
	arm_write_banked_reg(LR_svc, 0);
	arm_write_banked_reg(LR_abt, 0);
	arm_write_banked_reg(LR_und, 0);
	arm_write_banked_reg(LR_irq, 0);
	arm_write_banked_reg(LR_fiq, 0);
	arm_write_banked_reg(R8_fiq, 0);
	arm_write_banked_reg(R9_fiq, 0);
	arm_write_banked_reg(R10_fiq, 0);
	arm_write_banked_reg(R11_fiq, 0);
	arm_write_banked_reg(R12_fiq, 0);
	arm_write_banked_reg(SPSR_svc, 0);
	arm_write_banked_reg(SPSR_abt, 0);
	arm_write_banked_reg(SPSR_und, 0);
	arm_write_banked_reg(SPSR_irq, 0);
	arm_write_banked_reg(SPSR_fiq, 0);

	/* Wipe the system registers */
	arm_read_sysreg(SCTLR_EL1, sctlr);
	sctlr = sctlr & ~SCTLR_MASK;
	arm_write_sysreg(SCTLR_EL1, sctlr);
	arm_write_sysreg(CPACR_EL1, 0);
	arm_write_sysreg(CONTEXTIDR_EL1, 0);
	arm_write_sysreg(PAR_EL1, 0);
	arm_write_sysreg(TTBR0_EL1, 0);
	arm_write_sysreg(TTBR1_EL1, 0);
	arm_write_sysreg(CSSELR_EL1, 0);

	arm_write_sysreg(CNTKCTL_EL1, 0);
	arm_write_sysreg(CNTP_CTL_EL0, 0);
	arm_write_sysreg(CNTP_CVAL_EL0, 0);
	arm_write_sysreg(CNTV_CTL_EL0, 0);
	arm_write_sysreg(CNTV_CVAL_EL0, 0);

	/* AArch32 specific */
	arm_write_sysreg(TTBCR, 0);
	arm_write_sysreg(DACR, 0);
	arm_write_sysreg(VBAR, 0);
	arm_write_sysreg(DFSR, 0);
	arm_write_sysreg(DFAR, 0);
	arm_write_sysreg(IFSR, 0);
	arm_write_sysreg(IFAR, 0);
	arm_write_sysreg(ADFSR, 0);
	arm_write_sysreg(AIFSR, 0);
	arm_write_sysreg(MAIR0, 0);
	arm_write_sysreg(MAIR1, 0);
	arm_write_sysreg(AMAIR0, 0);
	arm_write_sysreg(AMAIR1, 0);
	arm_write_sysreg(TPIDRURW, 0);
	arm_write_sysreg(TPIDRURO, 0);
	arm_write_sysreg(TPIDRPRW, 0);

	arm_write_banked_reg(SPSR_hyp, RESET_PSR);
	arm_write_banked_reg(ELR_hyp, pc);

	/* transfer the context that may have been passed to PSCI_CPU_ON */
	regs->usr[1] = cpu_data->cpu_on_context;

	arm_paging_vcpu_init(&cell->arch.mm);

	irqchip_cpu_reset(cpu_data);
}

#ifdef CONFIG_CRASH_CELL_ON_PANIC
void arch_panic_park(void)
{
	arm_write_banked_reg(ELR_hyp, 0);
}
#endif

static void arch_dump_exit(struct registers *regs, const char *reason)
{
	unsigned long pc;
	unsigned int n;

	arm_read_banked_reg(ELR_hyp, pc);
	panic_printk("Unhandled HYP %s exit at 0x%lx\n", reason, pc);
	for (n = 0; n < NUM_USR_REGS; n++)
		panic_printk("r%d:%s 0x%08lx%s", n, n < 10 ? " " : "",
			     regs->usr[n], n % 4 == 3 ? "\n" : "  ");
	panic_printk("\n");
}

static void arch_dump_abt(bool is_data)
{
	u32 hxfar;
	u32 esr;

	arm_read_sysreg(ESR_EL2, esr);
	if (is_data)
		arm_read_sysreg(HDFAR, hxfar);
	else
		arm_read_sysreg(HIFAR, hxfar);

	panic_printk("Physical address: 0x%08x ESR: 0x%08x\n", hxfar, esr);
}

struct registers* arch_handle_exit(struct per_cpu *cpu_data,
				   struct registers *regs)
{
	cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_TOTAL]++;

	switch (regs->exit_reason) {
	case EXIT_REASON_IRQ:
		irqchip_handle_irq(cpu_data);
		break;
	case EXIT_REASON_TRAP:
		arch_handle_trap(regs);
		break;

	case EXIT_REASON_UNDEF:
		arch_dump_exit(regs, "undef");
		panic_stop();
	case EXIT_REASON_DABT:
		arch_dump_exit(regs, "data abort");
		arch_dump_abt(true);
		panic_stop();
	case EXIT_REASON_PABT:
		arch_dump_exit(regs, "prefetch abort");
		arch_dump_abt(false);
		panic_stop();
	case EXIT_REASON_HVC:
		arch_dump_exit(regs, "hvc");
		panic_stop();
	case EXIT_REASON_FIQ:
		arch_dump_exit(regs, "fiq");
		panic_stop();
	default:
		arch_dump_exit(regs, "unknown");
		panic_stop();
	}

	return regs;
}
