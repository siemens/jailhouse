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
#include <jailhouse/processor.h>
#include <jailhouse/string.h>
#include <asm/control.h>
#include <asm/irqchip.h>
#include <asm/processor.h>
#include <asm/sysregs.h>
#include <asm/traps.h>

static void arch_reset_el1(struct registers *regs)
{
	u32 sctlr;

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
}

void arch_reset_self(struct per_cpu *cpu_data)
{
	int err = 0;
	unsigned long reset_address;
	struct cell *cell = cpu_data->cell;
	struct registers *regs = guest_regs(cpu_data);
	bool is_shutdown = cpu_data->shutdown;

	if (!is_shutdown)
		err = arch_mmu_cpu_cell_init(cpu_data);
	if (err)
		printk("MMU setup failed\n");
	/*
	 * On the first CPU to reach this, write all cell datas to memory so it
	 * can be started with caches disabled.
	 * On all CPUs, invalidate the instruction caches to take into account
	 * the potential new instructions.
	 */
	arch_cell_caches_flush(cell);

	/*
	 * We come from the IRQ handler, but we won't return there, so the IPI
	 * is deactivated here.
	 */
	irqchip_eoi_irq(SGI_CPU_OFF, true);

	/* irqchip_cpu_shutdown already resets the GIC on all CPUs. */
	if (!is_shutdown) {
		err = irqchip_cpu_reset(cpu_data);
		if (err)
			printk("IRQ setup failed\n");
	}

	/* Wait for the driver to call cpu_up */
	if (cell == &root_cell || is_shutdown)
		reset_address = arch_smp_spin(cpu_data, root_cell.arch.smp);
	else
		reset_address = arch_smp_spin(cpu_data, cell->arch.smp);

	/* Set the new MPIDR */
	arm_write_sysreg(VMPIDR_EL2, cpu_data->virt_id | MPIDR_MP_BIT);

	/* Restore an empty context */
	arch_reset_el1(regs);

	arm_write_banked_reg(ELR_hyp, reset_address);
	arm_write_banked_reg(SPSR_hyp, RESET_PSR);

	if (is_shutdown)
		/* Won't return here. */
		arch_shutdown_self(cpu_data);

	vmreturn(regs);
}

static void arch_suspend_self(struct per_cpu *cpu_data)
{
	psci_suspend(cpu_data);

	if (cpu_data->flush_vcpu_caches)
		arch_cpu_tlb_flush(cpu_data);
}

static void arch_dump_exit(const char *reason)
{
	unsigned long pc;

	arm_read_banked_reg(ELR_hyp, pc);
	printk("Unhandled HYP %s exit at 0x%x\n", reason, pc);
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

	printk("  paddr=0x%lx esr=0x%x\n", hxfar, esr);
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

	case EXIT_REASON_UNDEF:
		arch_dump_exit("undef");
		panic_stop();
	case EXIT_REASON_DABT:
		arch_dump_exit("data abort");
		arch_dump_abt(true);
		panic_stop();
	case EXIT_REASON_PABT:
		arch_dump_exit("prefetch abort");
		arch_dump_abt(false);
		panic_stop();
	case EXIT_REASON_HVC:
		arch_dump_exit("hvc");
		panic_stop();
	case EXIT_REASON_FIQ:
		arch_dump_exit("fiq");
		panic_stop();
	default:
		arch_dump_exit("unknown");
		panic_stop();
	}

	if (cpu_data->shutdown)
		/* Won't return here. */
		arch_shutdown_self(cpu_data);

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
	struct per_cpu *cpu_data = per_cpu(cpu_id);

	/*
	 * Reset always follows park_cpu, so we just need to make sure that the
	 * CPU is suspended
	 */
	if (psci_wait_cpu_stopped(cpu_id) != 0)
		printk("ERROR: CPU%d is supposed to be stopped\n", cpu_id);
	else
		cpu_data->cell->arch.needs_flush = true;
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

unsigned int arm_cpu_virt2phys(struct cell *cell, unsigned int virt_id)
{
	unsigned int cpu;

	for_each_cpu(cpu, cell->cpu_set) {
		if (per_cpu(cpu)->virt_id == virt_id)
			return cpu;
	}

	return -1;
}

int arch_cell_create(struct cell *cell)
{
	int err;
	unsigned int cpu;
	unsigned int virt_id = 0;

	err = arch_mmu_cell_init(cell);
	if (err)
		return err;

	/*
	 * Generate a virtual CPU id according to the position of each CPU in
	 * the cell set
	 */
	for_each_cpu(cpu, cell->cpu_set) {
		per_cpu(cpu)->virt_id = virt_id;
		virt_id++;
	}
	cell->arch.last_virt_id = virt_id - 1;

	irqchip_cell_init(cell);
	irqchip_root_cell_shrink(cell);

	register_smp_ops(cell);

	return 0;
}

void arch_cell_destroy(struct cell *cell)
{
	unsigned int cpu;
	struct per_cpu *percpu;

	arch_mmu_cell_destroy(cell);

	for_each_cpu(cpu, cell->cpu_set) {
		percpu = per_cpu(cpu);
		/* Re-assign the physical IDs for the root cell */
		percpu->virt_id = percpu->cpu_id;
		arch_reset_cpu(cpu);
	}

	irqchip_cell_exit(cell);
}

/* Note: only supports synchronous flushing as triggered by config_commit! */
void arch_flush_cell_vcpu_caches(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu(cpu, cell->cpu_set)
		if (cpu == this_cpu_id())
			arch_cpu_tlb_flush(per_cpu(cpu));
		else
			per_cpu(cpu)->flush_vcpu_caches = true;
}

void arch_config_commit(struct cell *cell_added_removed)
{
}

void arch_panic_stop(void)
{
	psci_cpu_off(this_cpu_data());
	__builtin_unreachable();
}

void arch_panic_park(void)
{
	/* Won't return to panic_park */
	if (phys_processor_id() == panic_cpu)
		panic_in_progress = 0;

	psci_cpu_off(this_cpu_data());
	__builtin_unreachable();
}

/*
 * This handler is only used for cells, not for the root. The core already
 * issued a cpu_suspend. arch_reset_cpu will cause arch_reset_self to be
 * called on that CPU, which will in turn call arch_shutdown_self.
 */
void arch_shutdown_cpu(unsigned int cpu_id)
{
	struct per_cpu *cpu_data = per_cpu(cpu_id);

	cpu_data->virt_id = cpu_id;
	cpu_data->shutdown = true;

	if (psci_wait_cpu_stopped(cpu_id))
		printk("FATAL: unable to stop CPU%d\n", cpu_id);

	arch_reset_cpu(cpu_id);
}

void arch_shutdown(void)
{
	unsigned int cpu;
	struct cell *cell = root_cell.next;

	/* Re-route each SPI to CPU0 */
	for (; cell != NULL; cell = cell->next)
		irqchip_cell_exit(cell);

	/*
	 * Let the exit handler call reset_self to let the core finish its
	 * shutdown function and release its lock.
	 */
	for_each_cpu(cpu, root_cell.cpu_set)
		per_cpu(cpu)->shutdown = true;
}
