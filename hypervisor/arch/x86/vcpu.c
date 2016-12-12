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

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/pci.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/types.h>
#include <asm/apic.h>
#include <asm/i8042.h>
#include <asm/ioapic.h>
#include <asm/pci.h>
#include <asm/percpu.h>
#include <asm/vcpu.h>

/* This page is mapped so the code begins at 0x000ffff0 */
static u8 __attribute__((aligned(PAGE_SIZE))) parking_code[PAGE_SIZE] = {
	[0xff0] = 0xfa, /* 1: cli */
	[0xff1] = 0xf4, /*    hlt */
	[0xff2] = 0xeb,
	[0xff3] = 0xfc  /*    jmp 1b */
};

struct paging_structures parking_pt;

int vcpu_early_init(void)
{
	int err;

	err = vcpu_vendor_early_init();
	if (err)
		return err;

	/* Map guest parking code (shared between cells and CPUs) */
	parking_pt.root_table = page_alloc(&mem_pool, 1);
	if (!parking_pt.root_table)
		return -ENOMEM;
	return paging_create(&parking_pt, paging_hvirt2phys(parking_code),
			     PAGE_SIZE, 0x000ff000,
			     PAGE_READONLY_FLAGS | PAGE_FLAG_US,
			     PAGING_NON_COHERENT);
}

/* Can be overridden in vendor-specific code if needed */
const u8 *vcpu_get_inst_bytes(const struct guest_paging_structures *pg_structs,
			      unsigned long pc, unsigned int *size)
	__attribute__((weak, alias("vcpu_map_inst")));

const u8 *vcpu_map_inst(const struct guest_paging_structures *pg_structs,
			unsigned long pc, unsigned int *size)
{
	unsigned short bytes_avail;
	u8 *page = NULL;

	if (!*size)
		goto out_err;
	page = paging_get_guest_pages(pg_structs, pc,
			1, PAGE_READONLY_FLAGS);
	if (!page)
		goto out_err;

	/* Number of bytes available before page boundary */
	bytes_avail = PAGE_SIZE - (pc & PAGE_OFFS_MASK);
	if (*size > bytes_avail)
		*size = bytes_avail;

	return &page[pc & PAGE_OFFS_MASK];

out_err:
	return NULL;
}

int vcpu_cell_init(struct cell *cell)
{
	const u8 *pio_bitmap = jailhouse_cell_pio_bitmap(cell->config);
	u32 pio_bitmap_size = cell->config->pio_bitmap_size;
	struct vcpu_io_bitmap cell_iobm, root_cell_iobm;
	unsigned int n, pm_timer_addr;
	u32 size;
	int err;
	u8 *b;

	/* PM timer has to be provided */
	if (system_config->platform_info.x86.pm_timer_address == 0)
		return trace_error(-EINVAL);

	err = vcpu_vendor_cell_init(cell);
	if (err)
		return err;

	vcpu_vendor_get_cell_io_bitmap(cell, &cell_iobm);

	/* initialize io bitmap to trap all accesses */
	memset(cell_iobm.data, -1, cell_iobm.size);

	/* copy io bitmap from cell config */
	size = pio_bitmap_size > cell_iobm.size ?
			cell_iobm.size : pio_bitmap_size;
	memcpy(cell_iobm.data, pio_bitmap, size);

	/* moderate access to i8042 command register */
	cell_iobm.data[I8042_CMD_REG / 8] |= 1 << (I8042_CMD_REG % 8);

	if (cell != &root_cell) {
		/*
		 * Shrink PIO access of root cell corresponding to new cell's
		 * access rights.
		 */
		vcpu_vendor_get_cell_io_bitmap(&root_cell, &root_cell_iobm);
		pio_bitmap = jailhouse_cell_pio_bitmap(cell->config);
		for (b = root_cell_iobm.data; pio_bitmap_size > 0;
		     b++, pio_bitmap++, pio_bitmap_size--)
			*b |= ~*pio_bitmap;
	}

	/* permit access to the PM timer */
	pm_timer_addr = system_config->platform_info.x86.pm_timer_address;
	for (n = 0; n < 4; n++, pm_timer_addr++) {
		b = cell_iobm.data;
		b[pm_timer_addr / 8] &= ~(1 << (pm_timer_addr % 8));
	}

	return 0;
}

void vcpu_cell_exit(struct cell *cell)
{
	const u8 *root_pio_bitmap =
		jailhouse_cell_pio_bitmap(root_cell.config);
	const u8 *pio_bitmap = jailhouse_cell_pio_bitmap(cell->config);
	u32 pio_bitmap_size = cell->config->pio_bitmap_size;
	struct vcpu_io_bitmap root_cell_iobm;
	u8 *b;

	vcpu_vendor_get_cell_io_bitmap(&root_cell, &root_cell_iobm);

	if (root_cell.config->pio_bitmap_size < pio_bitmap_size)
		pio_bitmap_size = root_cell.config->pio_bitmap_size;

	for (b = root_cell_iobm.data; pio_bitmap_size > 0;
	     b++, pio_bitmap++, root_pio_bitmap++, pio_bitmap_size--)
		*b &= *pio_bitmap | *root_pio_bitmap;

	vcpu_vendor_cell_exit(cell);
}

void vcpu_handle_hypercall(void)
{
	union registers *guest_regs = &this_cpu_data()->guest_regs;
	unsigned long code = guest_regs->rax;
	struct vcpu_execution_state x_state;
	unsigned long arg_mask;
	bool long_mode;

	vcpu_skip_emulated_instruction(X86_INST_LEN_HYPERCALL);

	vcpu_vendor_get_execution_state(&x_state);

	long_mode = !!(x_state.efer & EFER_LMA);
	arg_mask = long_mode ? (u64)-1 : (u32)-1;

	if ((!long_mode && (x_state.rflags & X86_RFLAGS_VM)) ||
	    (x_state.cs & 3) != 0) {
		guest_regs->rax = -EPERM;
		return;
	}

	guest_regs->rax = hypercall(code, guest_regs->rdi & arg_mask,
				    guest_regs->rsi & arg_mask);
	if (guest_regs->rax == -ENOSYS)
		printk("CPU %d: Unknown hypercall %ld, RIP: 0x%016llx\n",
		       this_cpu_id(), code,
		       x_state.rip - X86_INST_LEN_HYPERCALL);

	if (code == JAILHOUSE_HC_DISABLE && guest_regs->rax == 0)
		vcpu_deactivate_vmm();
}

bool vcpu_handle_io_access(void)
{
	struct vcpu_io_intercept io;
	int result = 0;

	vcpu_vendor_get_io_intercept(&io);

	/* string and REP-prefixed instructions are not supported */
	if (io.rep_or_str)
		goto invalid_access;

	result = x86_pci_config_handler(io.port, io.in, io.size);
	if (result == 0)
		result = i8042_access_handler(io.port, io.in, io.size);

	if (result == 1) {
		vcpu_skip_emulated_instruction(io.inst_len);
		return true;
	}

invalid_access:
	/* report only unhandled access failures */
	if (result == 0)
		panic_printk("FATAL: Invalid PIO %s, port: %x size: %d\n",
			     io.in ? "read" : "write", io.port, io.size);
	return false;
}

bool vcpu_handle_mmio_access(void)
{
	union registers *guest_regs = &this_cpu_data()->guest_regs;
	enum mmio_result result = MMIO_UNHANDLED;
	struct guest_paging_structures pg_structs;
	struct mmio_access mmio = {.size = 0};
	struct vcpu_mmio_intercept intercept;
	struct vcpu_execution_state x_state;
	struct mmio_instruction inst;

	vcpu_vendor_get_execution_state(&x_state);
	vcpu_vendor_get_mmio_intercept(&intercept);

	if (!vcpu_get_guest_paging_structs(&pg_structs))
		goto invalid_access;

	inst = x86_mmio_parse(x_state.rip, &pg_structs, intercept.is_write);
	if (!inst.inst_len)
		goto invalid_access;

	mmio.is_write = intercept.is_write;
	if (mmio.is_write)
		mmio.value = guest_regs->by_index[inst.reg_num];

	mmio.address = intercept.phys_addr;
	mmio.size = inst.access_size;

	result = mmio_handle_access(&mmio);
	if (result == MMIO_HANDLED) {
		if (!mmio.is_write)
			guest_regs->by_index[inst.reg_num] = mmio.value;
		vcpu_skip_emulated_instruction(inst.inst_len);
		return true;
	}

invalid_access:
	/* report only unhandled access failures */
	if (result == MMIO_UNHANDLED)
		panic_printk("FATAL: Invalid MMIO/RAM %s, "
			     "addr: 0x%016llx size: %d\n",
			     intercept.is_write ? "write" : "read",
			     intercept.phys_addr, mmio.size);
	return false;
}

bool vcpu_handle_msr_read(void)
{
	struct per_cpu *cpu_data = this_cpu_data();

	switch (cpu_data->guest_regs.rcx) {
	case MSR_X2APIC_BASE ... MSR_X2APIC_END:
		x2apic_handle_read();
		break;
	case MSR_IA32_PAT:
		set_rdmsr_value(&cpu_data->guest_regs, cpu_data->pat);
		break;
	case MSR_IA32_MTRR_DEF_TYPE:
		set_rdmsr_value(&cpu_data->guest_regs,
				cpu_data->mtrr_def_type);
		break;
	default:
		panic_printk("FATAL: Unhandled MSR read: %lx\n",
			     cpu_data->guest_regs.rcx);
		return false;
	}

	vcpu_skip_emulated_instruction(X86_INST_LEN_WRMSR);
	return true;
}

bool vcpu_handle_msr_write(void)
{
	struct per_cpu *cpu_data = this_cpu_data();
	unsigned int bit_pos, pa;
	unsigned long val;

	switch (cpu_data->guest_regs.rcx) {
	case MSR_X2APIC_BASE ... MSR_X2APIC_END:
		if (!x2apic_handle_write())
			return false;
		break;
	case MSR_IA32_PAT:
		val = get_wrmsr_value(&cpu_data->guest_regs);
		for (bit_pos = 0; bit_pos < 64; bit_pos += 8) {
			pa = (val >> bit_pos) & 0xff;
			/* filter out reserved memory types */
			if (pa == 2 || pa == 3 || pa > 7) {
				printk("FATAL: Invalid PAT value: %lx\n", val);
				return false;
			}
		}
		cpu_data->pat = val;
		if (cpu_data->mtrr_def_type & MTRR_ENABLE)
			vcpu_vendor_set_guest_pat(val);
		break;
	case MSR_IA32_MTRR_DEF_TYPE:
		/*
		 * This only emulates the difference between MTRRs enabled
		 * and disabled. When disabled, we turn off all caching by
		 * setting the guest PAT to 0. When enabled, guest PAT +
		 * host-controlled MTRRs define the guest's memory types.
		 */
		val = get_wrmsr_value(&cpu_data->guest_regs);
		cpu_data->mtrr_def_type &= ~MTRR_ENABLE;
		cpu_data->mtrr_def_type |= val & MTRR_ENABLE;
		vcpu_vendor_set_guest_pat((val & MTRR_ENABLE) ?
					  cpu_data->pat : 0);
		break;
	default:
		panic_printk("FATAL: Unhandled MSR write: %lx\n",
			     cpu_data->guest_regs.rcx);
		return false;
	}

	vcpu_skip_emulated_instruction(X86_INST_LEN_WRMSR);
	return true;
}

void vcpu_handle_cpuid(void)
{
	static const char signature[12] = "Jailhouse";
	union registers *guest_regs = &this_cpu_data()->guest_regs;
	u32 function = guest_regs->rax;

	this_cpu_data()->stats[JAILHOUSE_CPU_STAT_VMEXITS_CPUID]++;

	switch (function) {
	case JAILHOUSE_CPUID_SIGNATURE:
		guest_regs->rax = JAILHOUSE_CPUID_FEATURES;
		guest_regs->rbx = *(u32 *)signature;
		guest_regs->rcx = *(u32 *)(signature + 4);
		guest_regs->rdx = *(u32 *)(signature + 8);
		break;
	case JAILHOUSE_CPUID_FEATURES:
		guest_regs->rax = 0;
		guest_regs->rbx = 0;
		guest_regs->rcx = 0;
		guest_regs->rdx = 0;
		break;
	default:
		/* clear upper 32 bits of the involved registers */
		guest_regs->rax &= 0xffffffff;
		guest_regs->rbx &= 0xffffffff;
		guest_regs->rcx &= 0xffffffff;
		guest_regs->rdx &= 0xffffffff;

		cpuid((u32 *)&guest_regs->rax, (u32 *)&guest_regs->rbx,
		      (u32 *)&guest_regs->rcx, (u32 *)&guest_regs->rdx);
		if (function == 0x01) {
			if (this_cell() != &root_cell)
				guest_regs->rcx |= X86_FEATURE_HYPERVISOR;

			guest_regs->rcx &= ~X86_FEATURE_OSXSAVE;
			if (vcpu_vendor_get_guest_cr4() & X86_CR4_OSXSAVE)
				guest_regs->rcx |= X86_FEATURE_OSXSAVE;
		}
		break;
	}

	vcpu_skip_emulated_instruction(X86_INST_LEN_CPUID);
}

void vcpu_reset(unsigned int sipi_vector)
{
	struct per_cpu *cpu_data = this_cpu_data();

	vcpu_vendor_reset(sipi_vector);

	memset(&cpu_data->guest_regs, 0, sizeof(cpu_data->guest_regs));

	if (sipi_vector == APIC_BSP_PSEUDO_SIPI) {
		cpu_data->pat = PAT_RESET_VALUE;
		cpu_data->mtrr_def_type &= ~MTRR_ENABLE;
		vcpu_vendor_set_guest_pat(0);
	}
}
