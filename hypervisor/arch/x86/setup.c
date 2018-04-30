/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/entry.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/apic.h>
#include <asm/bitops.h>
#include <asm/vcpu.h>

#define IDT_PRESENT_INT		0x00008e00

#define NUM_IDT_DESC		256
#define NUM_EXCP_DESC		20
#define IRQ_DESC_START		32

static u64 gdt[NUM_GDT_DESC] = {
	[GDT_DESC_NULL]   = 0,
	[GDT_DESC_CODE]   = 0x00af9b000000ffffUL,
	[GDT_DESC_TSS]    = 0x0000890000000000UL,
	[GDT_DESC_TSS_HI] = 0x0000000000000000UL,
};

extern u8 exception_entries[];
extern u8 nmi_entry[];
extern u8 irq_entry[];

unsigned long cache_line_size;
static u32 idt[NUM_IDT_DESC * 4];

static void set_idt_int_gate(unsigned int vector, unsigned long entry)
{
	idt[vector * 4] = (entry & 0xffff) | ((GDT_DESC_CODE * 8) << 16);
	idt[vector * 4 + 1] = IDT_PRESENT_INT | (entry & 0xffff0000);
	idt[vector * 4 + 2] = entry >> 32;
}

int arch_init_early(void)
{
	unsigned long entry;
	unsigned int vector;
	int err;

	cache_line_size = (cpuid_ebx(1, 0) & 0xff00) >> 5;

	err = apic_init();
	if (err)
		return err;

	entry = (unsigned long)exception_entries;
	for (vector = 0; vector < NUM_EXCP_DESC; vector++) {
		if (vector == NMI_VECTOR || vector == 15)
			continue;
		set_idt_int_gate(vector, entry);
		entry += 16;
	}

	set_idt_int_gate(NMI_VECTOR, (unsigned long)nmi_entry);

	for (vector = IRQ_DESC_START; vector < NUM_IDT_DESC; vector++)
		set_idt_int_gate(vector, (unsigned long)irq_entry);

	return vcpu_early_init();
}

/*
 * TODO: Current struct segment is VMX-specific (with 32-bit access rights).
 * We need a generic struct segment for x86 that is converted to VMX/SVM one
 * in the vmx.c/svm.c.
 */
static void read_descriptor(struct per_cpu *cpu_data, struct segment *seg)
{
	u64 *desc = (u64 *)(cpu_data->linux_gdtr.base +
			    (seg->selector & 0xfff8));

	if (desc[0] & DESC_PRESENT) {
		seg->base = ((desc[0] >> 16) & 0xffffff) |
			((desc[0] >> 32) & 0xff000000);
		if (!(desc[0] & DESC_CODE_DATA))
			seg->base |= desc[1] << 32;

		seg->limit = (desc[0] & 0xffff) | ((desc[0] >> 32) & 0xf0000);
		if (desc[0] & DESC_PAGE_GRAN)
			seg->limit = (seg->limit << 12) | 0xfff;

		seg->access_rights = (desc[0] >> 40) & 0xf0ff;
	} else {
		seg->base = 0;
		seg->limit = 0;
		seg->access_rights = 0x10000;
	}
}

static void set_cs(u16 cs)
{
	asm volatile(
		"lea 1f(%%rip),%%rax\n\t"
		"push %0\n\t"
		"push %%rax\n\t"
		"lretq\n\t"
		"1:"
		: : "m" (cs) : "rax");
}

int arch_cpu_init(struct per_cpu *cpu_data)
{
	struct desc_table_reg dtr;
	int err, n;

	/* read GDTR */
	read_gdtr(&cpu_data->linux_gdtr);

	/* read TR and TSS descriptor */
	asm volatile("str %0" : "=m" (cpu_data->linux_tss.selector));
	read_descriptor(cpu_data, &cpu_data->linux_tss);

	if (cpu_data->linux_tss.selector / 8 >= NUM_GDT_DESC)
		return trace_error(-EINVAL);

	/* save CS as long as we have access to the Linux page table */
	asm volatile("mov %%cs,%0" : "=m" (cpu_data->linux_cs.selector));
	read_descriptor(cpu_data, &cpu_data->linux_cs);

	/* save segment registers - they may point to 32 or 16 bit segments */
	asm volatile("mov %%ds,%0" : "=m" (cpu_data->linux_ds.selector));
	read_descriptor(cpu_data, &cpu_data->linux_ds);

	asm volatile("mov %%es,%0" : "=m" (cpu_data->linux_es.selector));
	read_descriptor(cpu_data, &cpu_data->linux_es);

	asm volatile("mov %%fs,%0" : "=m" (cpu_data->linux_fs.selector));
	read_descriptor(cpu_data, &cpu_data->linux_fs);
	cpu_data->linux_fs.base = read_msr(MSR_FS_BASE);

	asm volatile("mov %%gs,%0" : "=m" (cpu_data->linux_gs.selector));
	read_descriptor(cpu_data, &cpu_data->linux_gs);
	cpu_data->linux_gs.base = read_msr(MSR_GS_BASE);

	/* read registers to restore on first VM-entry */
	for (n = 0; n < NUM_ENTRY_REGS; n++)
		cpu_data->linux_reg[n] =
			((unsigned long *)cpu_data->linux_sp)[n];
	cpu_data->linux_ip =
		((unsigned long *)cpu_data->linux_sp)[NUM_ENTRY_REGS];

	/* set GDTR */
	dtr.limit = NUM_GDT_DESC * 8 - 1;
	dtr.base = (u64)&gdt;
	write_gdtr(&dtr);

	set_cs(GDT_DESC_CODE * 8);

	/* swap IDTR */
	read_idtr(&cpu_data->linux_idtr);
	dtr.limit = NUM_IDT_DESC * 16 - 1;
	dtr.base = (u64)&idt;
	write_idtr(&dtr);

	/* paranoid clearing of segment registers */
	asm volatile(
		"mov %0,%%es\n\t"
		"mov %0,%%ds\n\t"
		"mov %0,%%ss"
		: : "r" (0));

	/* clear TSS busy flag set by previous loading, then set TR */
	gdt[GDT_DESC_TSS] &= ~DESC_TSS_BUSY;
	asm volatile("ltr %%ax" : : "a" (GDT_DESC_TSS * 8));

	cpu_data->linux_cr0 = read_cr0();
	cpu_data->linux_cr4 = read_cr4();

	/* swap CR3 */
	cpu_data->linux_cr3 = read_cr3();
	write_cr3(paging_hvirt2phys(cpu_data->pg_structs.root_table));

	cpu_data->pat = read_msr(MSR_IA32_PAT);
	write_msr(MSR_IA32_PAT, PAT_RESET_VALUE);

	cpu_data->mtrr_def_type = read_msr(MSR_IA32_MTRR_DEF_TYPE);

	cpu_data->linux_efer = read_msr(MSR_EFER);

	cpu_data->initialized = true;

	err = apic_cpu_init(cpu_data);
	if (err)
		goto error_out;

	err = vcpu_init(cpu_data);
	if (err)
		goto error_out;

	return 0;

error_out:
	arch_cpu_restore(this_cpu_id(), err);
	return err;
}

void __attribute__((noreturn)) arch_cpu_activate_vmm(void)
{
	vcpu_activate_vmm();
}

void arch_cpu_restore(unsigned int cpu_id, int return_code)
{
	static spinlock_t tss_lock;
	struct per_cpu *cpu_data = per_cpu(cpu_id);
	unsigned int tss_idx;
	u64 *linux_gdt;

	if (!cpu_data->initialized)
		return;

	vcpu_exit(cpu_data);

	write_msr(MSR_IA32_PAT, cpu_data->pat);
	write_msr(MSR_EFER, cpu_data->linux_efer);
	write_cr0(cpu_data->linux_cr0);
	write_cr4(cpu_data->linux_cr4);
	/* cr3 must be last in case cr4 enables PCID */
	write_cr3(cpu_data->linux_cr3);

	/*
	 * Copy Linux TSS descriptor into our GDT, clearing the busy flag,
	 * then reload TR from it. We can't use Linux' GDT as it is r/o.
	 * Access can happen concurrently on multiple CPUs, so we have to
	 * serialize the critical section.
	 */
	linux_gdt = (u64 *)cpu_data->linux_gdtr.base;
	tss_idx = cpu_data->linux_tss.selector / 8;

	spin_lock(&tss_lock);

	gdt[tss_idx] = linux_gdt[tss_idx] & ~DESC_TSS_BUSY;
	gdt[tss_idx + 1] = linux_gdt[tss_idx + 1];
	asm volatile("ltr %%ax" : : "a" (cpu_data->linux_tss.selector));

	spin_unlock(&tss_lock);

	asm volatile("lgdtq %0" : : "m" (cpu_data->linux_gdtr));
	asm volatile("lidtq %0" : : "m" (cpu_data->linux_idtr));

	set_cs(cpu_data->linux_cs.selector);

	asm volatile("mov %0,%%ds" : : "r" (cpu_data->linux_ds.selector));
	asm volatile("mov %0,%%es" : : "r" (cpu_data->linux_es.selector));
	asm volatile("mov %0,%%fs" : : "r" (cpu_data->linux_fs.selector));
	asm volatile(
		"swapgs\n\t"
		"mov %0,%%gs\n\t"
		"mfence\n\t"
		"swapgs\n\t"
		: : "r" (cpu_data->linux_gs.selector));

	write_msr(MSR_FS_BASE, cpu_data->linux_fs.base);
	write_msr(MSR_GS_BASE, cpu_data->linux_gs.base);
}
