/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <asm/control.h>
#include <asm/irqchip.h>
#include <asm/sysregs.h>
#include <asm/traps.h>

static void arch_reset_el1(struct registers *regs)
{
	/* Wipe all banked and usr regs */
	memset(regs, 0, sizeof(struct registers));

	arm_write_banked_reg(SP_usr, 0);
	arm_write_banked_reg(SP_svc, 0);
	arm_write_banked_reg(SP_abt, 0);
	arm_write_banked_reg(SP_und, 0);
	arm_write_banked_reg(SP_svc, 0);
	arm_write_banked_reg(SP_irq, 0);
	arm_write_banked_reg(SP_fiq, 0);
	arm_write_banked_reg(LR_svc, 0);
	arm_write_banked_reg(LR_abt, 0);
	arm_write_banked_reg(LR_und, 0);
	arm_write_banked_reg(LR_svc, 0);
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
	arm_write_banked_reg(SPSR_svc, 0);
	arm_write_banked_reg(SPSR_irq, 0);
	arm_write_banked_reg(SPSR_fiq, 0);

	/* Wipe the system registers */
	arm_write_sysreg(SCTLR_EL1, 0);
	arm_write_sysreg(ACTLR_EL1, 0);
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
}

static void arch_reset_self(struct per_cpu *cpu_data)
{
	int err;
	unsigned long reset_address;
	struct registers *regs = guest_regs(cpu_data);

	err = arch_mmu_cpu_cell_init(cpu_data);
	if (err)
		printk("MMU setup failed\n");

	/*
	 * We come from the IRQ handler, but we won't return there, so the IPI
	 * is deactivated here.
	 */
	irqchip_eoi_irq(SGI_CPU_OFF, true);

	err = irqchip_cpu_reset(cpu_data);
	if (err)
		printk("IRQ setup failed\n");

	if (cpu_data->cell == &root_cell)
		/* Wait for the driver to call cpu_up */
		reset_address = arch_cpu_spin();
	else
		reset_address = 0;

	/* Restore an empty context */
	arch_reset_el1(regs);

	arm_write_banked_reg(ELR_hyp, reset_address);
	arm_write_banked_reg(SPSR_hyp, RESET_PSR);

	vmreturn(regs);
}

static void arch_suspend_self(struct per_cpu *cpu_data)
{
	psci_suspend(cpu_data);
}

struct registers* arch_handle_exit(struct per_cpu *cpu_data,
				   struct registers *regs)
{
	switch (regs->exit_reason) {
	case EXIT_REASON_IRQ:
		irqchip_handle_irq(cpu_data);
		break;
	case EXIT_REASON_TRAP:
		arch_handle_trap(cpu_data, regs);
		break;
	default:
		printk("Internal error: %d exit not implemented\n",
				regs->exit_reason);
		while(1);
	}

	return regs;
}

/* CPU must be stopped */
void arch_resume_cpu(unsigned int cpu_id)
{
	/*
	 * Simply get out of the spin loop by returning to handle_sgi
	 * If the CPU is being reset, it already has left the PSCI idle loop.
	 */
	if (psci_cpu_stopped(cpu_id))
		psci_resume(cpu_id);
}

/* CPU must be stopped */
void arch_park_cpu(unsigned int cpu_id)
{
	/*
	 * Reset always follows park_cpu, so we just need to make sure that the
	 * CPU is suspended
	 */
	if (psci_wait_cpu_stopped(cpu_id) != 0)
		printk("ERROR: CPU%d is supposed to be stopped\n", cpu_id);
}

/* CPU must be stopped */
void arch_reset_cpu(unsigned int cpu_id)
{
	unsigned long cpu_data = (unsigned long)per_cpu(cpu_id);

	if (psci_cpu_on(cpu_id, (unsigned long)arch_reset_self, cpu_data))
		printk("ERROR: unable to reset CPU%d (was running)\n", cpu_id);
}

void arch_suspend_cpu(unsigned int cpu_id)
{
	struct sgi sgi;

	if (psci_cpu_stopped(cpu_id) != 0)
		return;

	sgi.routing_mode = 0;
	sgi.aff1 = 0;
	sgi.aff2 = 0;
	sgi.aff3 = 0;
	sgi.targets = 1 << cpu_id;
	sgi.id = SGI_CPU_OFF;

	irqchip_send_sgi(&sgi);
}

void arch_handle_sgi(struct per_cpu *cpu_data, u32 irqn)
{
	switch (irqn) {
	case SGI_INJECT:
		irqchip_inject_pending(cpu_data);
		break;
	case SGI_CPU_OFF:
		arch_suspend_self(cpu_data);
		break;
	default:
		printk("WARN: unknown SGI received %d\n", irqn);
	}
}

int arch_cell_create(struct cell *cell)
{
	int err;

	err = arch_mmu_cell_init(cell);
	if (err)
		return err;

	return 0;
}

void arch_cell_destroy(struct cell *cell)
{
	unsigned int cpu;

	arch_mmu_cell_destroy(cell);

	for_each_cpu(cpu, cell->cpu_set)
		arch_reset_cpu(cpu);
}
