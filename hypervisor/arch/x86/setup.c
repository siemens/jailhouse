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

#include <jailhouse/entry.h>
#include <jailhouse/paging.h>
#include <jailhouse/processor.h>
#include <asm/apic.h>
#include <asm/bitops.h>
#include <asm/vmx.h>
#include <asm/vtd.h>

#define TSS_BUSY_FLAG		(1UL << (9 + 32))

#define IDT_PRESENT_INT		0x00008e00

#define NUM_IDT_DESC		256
#define NUM_EXCP_DESC		20
#define IRQ_DESC_START		32

struct farptr {
	u64 offs;
	u16 seg;
} __attribute__((packed));

static u64 gdt[] = {
	[GDT_DESC_NULL]   = 0,
	[GDT_DESC_CODE]   = 0x00af9b000000ffff,
	[GDT_DESC_TSS]    = 0x0000890000000000,
	[GDT_DESC_TSS_HI] = 0x0000000000000000,
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

int arch_init_early(struct cell *linux_cell)
{
	unsigned long entry;
	unsigned int vector;
	int err;

	cache_line_size = (cpuid_ebx(1) & 0xff00) >> 5;

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

	vmx_init();

	err = vmx_cell_init(linux_cell);
	if (err)
		return err;

	return 0;
}

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
	struct farptr jmp_target;
	unsigned long tmp;

	jmp_target.seg = cs;
	asm volatile(
		"lea 1f(%%rip),%0\n\t"
		"mov %0,%1\n\t"
		"rex64/ljmp *%2\n\t"
		"1:"
		: "=r" (tmp) : "m" (jmp_target.offs), "m" (jmp_target));
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
	cpu_data->linux_ip = ((unsigned long *)cpu_data->linux_sp)[6];

	/* swap CR3 */
	cpu_data->linux_cr3 = read_cr3();
	write_cr3(page_map_hvirt2phys(hv_paging_structs.root_table));

	/* set GDTR */
	dtr.limit = NUM_GDT_DESC * 8 - 1;
	dtr.base = (u64)&gdt;
	write_gdtr(&dtr);

	set_cs(GDT_DESC_CODE * 8);

	/* paranoid clearing of segment registers */
	asm volatile(
		"mov %0,%%es\n\t"
		"mov %0,%%ds\n\t"
		"mov %0,%%ss"
		: : "r" (0));

	/* clear TSS busy flag set by previous loading, then set TR */
	gdt[GDT_DESC_TSS] &= ~TSS_BUSY_FLAG;
	asm volatile("ltr %%ax" : : "a" (GDT_DESC_TSS * 8));

	/* swap IDTR */
	read_idtr(&cpu_data->linux_idtr);
	dtr.limit = NUM_IDT_DESC * 16 - 1;
	dtr.base = (u64)&idt;
	write_idtr(&dtr);

	cpu_data->linux_efer = read_msr(MSR_EFER);

	cpu_data->linux_sysenter_cs = read_msr(MSR_IA32_SYSENTER_CS);
	cpu_data->linux_sysenter_eip = read_msr(MSR_IA32_SYSENTER_EIP);
	cpu_data->linux_sysenter_esp = read_msr(MSR_IA32_SYSENTER_ESP);

	cpu_data->initialized = true;

	err = apic_cpu_init(cpu_data);
	if (err)
		goto error_out;

	err = vmx_cpu_init(cpu_data);
	if (err)
		goto error_out;

	return 0;

error_out:
	arch_cpu_restore(cpu_data);
	return err;
}

int arch_init_late(struct cell *linux_cell)
{
	int err;

	err = vtd_init();
	if (err)
		return err;

	err = vtd_cell_init(linux_cell);
	if (err)
		return err;

	return 0;
}

void arch_cpu_activate_vmm(struct per_cpu *cpu_data)
{
	vmx_cpu_activate_vmm(cpu_data);
}

void arch_cpu_restore(struct per_cpu *cpu_data)
{
	u64 *gdt;

	if (!cpu_data->initialized)
		return;

	vmx_cpu_exit(cpu_data);

	write_msr(MSR_EFER, cpu_data->linux_efer);
	write_cr3(cpu_data->linux_cr3);

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

	/* clear busy flag in Linux TSS, then reload it */
	gdt = (u64 *)cpu_data->linux_gdtr.base;
	gdt[cpu_data->linux_tss.selector / 8] &= ~TSS_BUSY_FLAG;
	asm volatile("ltr %%ax" : : "a" (cpu_data->linux_tss.selector));

	write_msr(MSR_FS_BASE, cpu_data->linux_fs.base);
	write_msr(MSR_GS_BASE, cpu_data->linux_gs.base);

	write_msr(MSR_IA32_SYSENTER_CS, cpu_data->linux_sysenter_cs);
	write_msr(MSR_IA32_SYSENTER_EIP, cpu_data->linux_sysenter_eip);
	write_msr(MSR_IA32_SYSENTER_ESP, cpu_data->linux_sysenter_esp);
}
