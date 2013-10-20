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
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <jailhouse/control.h>
#include <asm/vmx.h>
#include <asm/apic.h>
#include <asm/bitops.h>

#define TSS_BUSY_FLAG		(1UL << (9 + 32))

#define NUM_IDT_DESC		20

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

static u32 idt[NUM_IDT_DESC * 4];

int arch_init_early(struct cell *linux_cell,
		    struct jailhouse_cell_desc *config)
{
	unsigned long entry;
	unsigned int vector;
	int err;

	err = apic_init();
	if (err)
		return err;

	entry = (unsigned long)exception_entries;
	for (vector = 0; vector < NUM_IDT_DESC; vector++) {
		if (vector == NMI_VECTOR || vector == 15)
			continue;
		idt[vector * 4] = (entry & 0xffff) |
			((GDT_DESC_CODE * 8) << 16);
		idt[vector * 4 + 1] = 0x8e00 | (entry & 0xffff0000);
		idt[vector * 4 + 2] = entry >> 32;
		entry += 16;
	}

	entry = (unsigned long)nmi_entry;
	idt[NMI_VECTOR * 4] = (entry & 0xffff) | ((GDT_DESC_CODE * 8) << 16);
	idt[NMI_VECTOR * 4 + 1] = 0x8e00 | (entry & 0xffff0000);
	idt[NMI_VECTOR * 4 + 2] = entry >> 32;

	vmx_init();

	err = vmx_cell_init(linux_cell, config);
	if (err)
		return err;

	return 0;
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
	u64 *linux_tr_desc;
	int err, n;

	/* read GDTR */
	read_gdtr(&cpu_data->linux_gdtr);

	/* read TR and TSS descriptor */
	asm volatile("str %0" : "=m" (cpu_data->linux_tr));
	linux_tr_desc = (u64 *)(cpu_data->linux_gdtr.base +
		(cpu_data->linux_tr & 0xfff8));
	cpu_data->linux_tr_base = ((linux_tr_desc[0] >> 16) & 0xffffff) |
		((linux_tr_desc[0] >> 32) & 0xff000000) |
		(linux_tr_desc[1] << 32);
	cpu_data->linux_tr_limit = (linux_tr_desc[0] & 0xffff) |
		((linux_tr_desc[0] >> 32) & 0xff0000);
	cpu_data->linux_tr_ar_bytes = (linux_tr_desc[0] >> 40) & 0xffff;

	/* read registers to restore on first VM-entry */
	for (n = 0; n < NUM_ENTRY_REGS; n++)
		cpu_data->linux_reg[n] =
			((unsigned long *)cpu_data->linux_sp)[n];
	cpu_data->linux_ip = ((unsigned long *)cpu_data->linux_sp)[6];

	/* swap CR3 */
	cpu_data->linux_cr3 = read_cr3();
	write_cr3(page_map_hvirt2phys(hv_page_table));

	/* set GDTR */
	dtr.limit = NUM_GDT_DESC * 8 - 1;
	dtr.base = (u64)&gdt;
	write_gdtr(&dtr);

	/* set CS */
	asm volatile("mov %%cs,%0": "=m" (cpu_data->linux_cs));
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
	cpu_data->linux_fs_base = read_msr(MSR_FS_BASE);
	cpu_data->linux_gs_base = read_msr(MSR_GS_BASE);

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

int arch_init_late(struct cell *linux_cell,
		   struct jailhouse_cell_desc *config)
{
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

	set_cs(cpu_data->linux_cs);

	/* clear busy flag in Linux TSS, then reload it */
	gdt = (u64 *)cpu_data->linux_gdtr.base;
	gdt[cpu_data->linux_tr / 8] &= ~TSS_BUSY_FLAG;
	asm volatile("ltr %%ax" : : "a" (cpu_data->linux_tr));

	write_msr(MSR_FS_BASE, cpu_data->linux_fs_base);
	write_msr(MSR_GS_BASE, cpu_data->linux_gs_base);

	write_msr(MSR_IA32_SYSENTER_CS, cpu_data->linux_sysenter_cs);
	write_msr(MSR_IA32_SYSENTER_EIP, cpu_data->linux_sysenter_eip);
	write_msr(MSR_IA32_SYSENTER_ESP, cpu_data->linux_sysenter_esp);
}
