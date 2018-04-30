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
 * Based on vmx.c written by Jan Kiszka.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/entry.h>
#include <jailhouse/cell.h>
#include <jailhouse/cell-config.h>
#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <jailhouse/string.h>
#include <jailhouse/utils.h>
#include <asm/amd_iommu.h>
#include <asm/apic.h>
#include <asm/control.h>
#include <asm/iommu.h>
#include <asm/paging.h>
#include <asm/percpu.h>
#include <asm/processor.h>
#include <asm/svm.h>
#include <asm/vcpu.h>

/*
 * NW bit is ignored by all modern processors, however some
 * combinations of NW and CD bits are prohibited by SVM (see APMv2,
 * Sect. 15.5). To handle this, we always keep the NW bit off.
 */
#define SVM_CR0_ALLOWED_BITS		(~X86_CR0_NW)

/* IOPM size: two 4-K pages + 3 bits */
#define IOPM_PAGES			3

#define NPT_IOMMU_PAGE_DIR_LEVELS	4

static bool has_avic, has_assists, has_flush_by_asid;

static const struct segment invalid_seg;

static struct paging npt_iommu_paging[NPT_IOMMU_PAGE_DIR_LEVELS];

/* bit cleared: direct access allowed */
// TODO: convert to whitelist
static u8 __attribute__((aligned(PAGE_SIZE))) msrpm[][0x2000/4] = {
	[ SVM_MSRPM_0000 ] = {
		[      0/4 ...  0x017/4 ] = 0,
		[  0x018/4 ...  0x01b/4 ] = 0x80, /* 0x01b (w) */
		[  0x01c/4 ...  0x1ff/4 ] = 0,
		[  0x200/4 ...  0x273/4 ] = 0xaa, /* 0x200 - 0x273 (w) */
		[  0x274/4 ...  0x277/4 ] = 0xea, /* 0x274 - 0x276 (w), 0x277 (rw) */
		[  0x278/4 ...  0x2fb/4 ] = 0,
		[  0x2fc/4 ...  0x2ff/4 ] = 0x80, /* 0x2ff (w) */
		[  0x300/4 ...  0x7ff/4 ] = 0,
		/* x2APIC MSRs - emulated if not present */
		[  0x800/4 ...  0x803/4 ] = 0x90, /* 0x802 (r), 0x803 (r) */
		[  0x804/4 ...  0x807/4 ] = 0,
		[  0x808/4 ...  0x80b/4 ] = 0x93, /* 0x808 (rw), 0x80a (r), 0x80b (w) */
		[  0x80c/4 ...  0x80f/4 ] = 0xc8, /* 0x80d (w), 0x80f (rw) */
		[  0x810/4 ...  0x813/4 ] = 0x55, /* 0x810 - 0x813 (r) */
		[  0x814/4 ...  0x817/4 ] = 0x55, /* 0x814 - 0x817 (r) */
		[  0x818/4 ...  0x81b/4 ] = 0x55, /* 0x818 - 0x81b (r) */
		[  0x81c/4 ...  0x81f/4 ] = 0x55, /* 0x81c - 0x81f (r) */
		[  0x820/4 ...  0x823/4 ] = 0x55, /* 0x820 - 0x823 (r) */
		[  0x824/4 ...  0x827/4 ] = 0x55, /* 0x823 - 0x827 (r) */
		[  0x828/4 ...  0x82b/4 ] = 0x03, /* 0x828 (rw) */
		[  0x82c/4 ...  0x82f/4 ] = 0xc0, /* 0x82f (rw) */
		[  0x830/4 ...  0x833/4 ] = 0xf3, /* 0x830 (rw), 0x832 (rw), 0x833 (rw) */
		[  0x834/4 ...  0x837/4 ] = 0xff, /* 0x834 - 0x837 (rw) */
		[  0x838/4 ...  0x83b/4 ] = 0x07, /* 0x838 (rw), 0x839 (r) */
		[  0x83c/4 ...  0x83f/4 ] = 0x70, /* 0x83e (rw), 0x83f (r) */
		[  0x840/4 ... 0x1fff/4 ] = 0,
	},
	[ SVM_MSRPM_C000 ] = {
		[      0/4 ...  0x07f/4 ] = 0,
		[  0x080/4 ...  0x083/4 ] = 0x02, /* 0x080 (w) */
		[  0x084/4 ... 0x1fff/4 ] = 0
	},
	[ SVM_MSRPM_C001 ] = {
		[      0/4 ... 0x1fff/4 ] = 0,
	},
	[ SVM_MSRPM_RESV ] = {
		[      0/4 ... 0x1fff/4 ] = 0,
	}
};

static void *avic_page;

static int svm_check_features(void)
{
	/* SVM is available */
	if (!(cpuid_ecx(0x80000001, 0) & X86_FEATURE_SVM))
		return trace_error(-ENODEV);

	/* Nested paging */
	if (!(cpuid_edx(0x8000000A, 0) & X86_FEATURE_NP))
		return trace_error(-EIO);

	/* Decode assists */
	if ((cpuid_edx(0x8000000A, 0) & X86_FEATURE_DECODE_ASSISTS))
		has_assists = true;

	/* AVIC support */
	/* FIXME: Jailhouse support is incomplete so far
	if (cpuid_edx(0x8000000A, 0) & X86_FEATURE_AVIC)
		has_avic = true; */

	/* TLB Flush by ASID support */
	if (cpuid_edx(0x8000000A, 0) & X86_FEATURE_FLUSH_BY_ASID)
		has_flush_by_asid = true;

	return 0;
}

static void set_svm_segment_from_dtr(struct svm_segment *svm_segment,
				     const struct desc_table_reg *dtr)
{
	svm_segment->base = dtr->base;
	svm_segment->limit = dtr->limit & 0xffff;
}

static void set_svm_segment_from_segment(struct svm_segment *svm_segment,
					 const struct segment *segment)
{
	svm_segment->selector = segment->selector;
	svm_segment->attributes = ((segment->access_rights & 0xf000) >> 4) |
		(segment->access_rights & 0x00ff);
	svm_segment->limit = segment->limit;
	svm_segment->base = segment->base;
}

static void svm_set_cell_config(struct cell *cell, struct vmcb *vmcb)
{
	vmcb->iopm_base_pa = paging_hvirt2phys(cell->arch.svm.iopm);
	vmcb->n_cr3 =
		paging_hvirt2phys(cell->arch.svm.npt_iommu_structs.root_table);
}

static void vmcb_setup(struct per_cpu *cpu_data)
{
	struct vmcb *vmcb = &cpu_data->vmcb;

	memset(vmcb, 0, sizeof(struct vmcb));

	vmcb->cr0 = cpu_data->linux_cr0 & SVM_CR0_ALLOWED_BITS;
	vmcb->cr3 = cpu_data->linux_cr3;
	vmcb->cr4 = cpu_data->linux_cr4;

	set_svm_segment_from_segment(&vmcb->cs, &cpu_data->linux_cs);
	set_svm_segment_from_segment(&vmcb->ds, &cpu_data->linux_ds);
	set_svm_segment_from_segment(&vmcb->es, &cpu_data->linux_es);
	set_svm_segment_from_segment(&vmcb->fs, &cpu_data->linux_fs);
	set_svm_segment_from_segment(&vmcb->gs, &cpu_data->linux_gs);
	set_svm_segment_from_segment(&vmcb->ss, &invalid_seg);
	set_svm_segment_from_segment(&vmcb->tr, &cpu_data->linux_tss);
	set_svm_segment_from_segment(&vmcb->ldtr, &invalid_seg);

	set_svm_segment_from_dtr(&vmcb->gdtr, &cpu_data->linux_gdtr);
	set_svm_segment_from_dtr(&vmcb->idtr, &cpu_data->linux_idtr);

	vmcb->cpl = 0; /* Linux runs in ring 0 before migration */

	vmcb->rflags = 0x02;
	/* Indicate success to the caller of arch_entry */
	vmcb->rax = 0;
	vmcb->rsp = cpu_data->linux_sp +
		(NUM_ENTRY_REGS + 1) * sizeof(unsigned long);
	vmcb->rip = cpu_data->linux_ip;

	vmcb->sysenter_cs = read_msr(MSR_IA32_SYSENTER_CS);
	vmcb->sysenter_eip = read_msr(MSR_IA32_SYSENTER_EIP);
	vmcb->sysenter_esp = read_msr(MSR_IA32_SYSENTER_ESP);
	vmcb->star = read_msr(MSR_STAR);
	vmcb->lstar = read_msr(MSR_LSTAR);
	vmcb->cstar = read_msr(MSR_CSTAR);
	vmcb->sfmask = read_msr(MSR_SFMASK);
	vmcb->kerngsbase = read_msr(MSR_KERNGS_BASE);

	vmcb->dr6 = 0x00000ff0;
	vmcb->dr7 = 0x00000400;

	/* Make the hypervisor visible */
	vmcb->efer = (cpu_data->linux_efer | EFER_SVME);

	vmcb->g_pat = cpu_data->pat;

	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_NMI;
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_CR0_SEL_WRITE;
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_CPUID;
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_IOIO_PROT;
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_MSR_PROT;
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_SHUTDOWN_EVT;

	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_VMRUN;
	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_VMMCALL;
	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_VMLOAD;
	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_VMSAVE;
	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_STGI;
	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_CLGI;
	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_SKINIT;

	/*
	 * We only intercept #DB and #AC to prevent that malicious guests can
	 * trigger infinite loops in microcode (see e.g. CVE-2015-5307 and
	 * CVE-2015-8104).
	 */
	vmcb->exception_intercepts |= (1 << DB_VECTOR) | (1 << AC_VECTOR);

	vmcb->msrpm_base_pa = paging_hvirt2phys(msrpm);

	vmcb->np_enable = 1;
	/* No more than one guest owns the CPU */
	vmcb->guest_asid = 1;

	/* TODO: Setup AVIC */

	/* Explicitly mark all of the state as new */
	vmcb->clean_bits = 0;

	svm_set_cell_config(cpu_data->cell, vmcb);
}

unsigned long arch_paging_gphys2phys(unsigned long gphys,
				     unsigned long flags)
{
	return paging_virt2phys(&this_cell()->arch.svm.npt_iommu_structs,
				gphys, flags);
}

static void npt_iommu_set_next_pt_l4(pt_entry_t pte, unsigned long next_pt)
{
	/*
	 * Merge IOMMU and NPT flags. We need to mark the NTP entries as user
	 * accessible, see APMv2, Section 15.25.5.
	 */
	*pte = (next_pt & BIT_MASK(51, 12)) | AMD_IOMMU_PTE_PG_MODE(3) |
		AMD_IOMMU_PTE_IR | AMD_IOMMU_PTE_IW | AMD_IOMMU_PTE_P |
		PAGE_DEFAULT_FLAGS | PAGE_FLAG_US;
}

static void npt_iommu_set_next_pt_l3(pt_entry_t pte, unsigned long next_pt)
{
	*pte = (next_pt & BIT_MASK(51, 12)) | AMD_IOMMU_PTE_PG_MODE(2) |
		AMD_IOMMU_PTE_IR | AMD_IOMMU_PTE_IW | AMD_IOMMU_PTE_P |
		PAGE_DEFAULT_FLAGS | PAGE_FLAG_US;
}

static void npt_iommu_set_next_pt_l2(pt_entry_t pte, unsigned long next_pt)
{
	*pte = (next_pt & BIT_MASK(51, 12)) | AMD_IOMMU_PTE_PG_MODE(1) |
		AMD_IOMMU_PTE_IR | AMD_IOMMU_PTE_IW | AMD_IOMMU_PTE_P |
		PAGE_DEFAULT_FLAGS | PAGE_FLAG_US;
}

static unsigned long npt_iommu_get_phys_l3(pt_entry_t pte, unsigned long virt)
{
	if (*pte & AMD_IOMMU_PTE_PG_MODE_MASK)
		return INVALID_PHYS_ADDR;
	return (*pte & BIT_MASK(51, 30)) | (virt & BIT_MASK(29, 0));
}

static unsigned long npt_iommu_get_phys_l2(pt_entry_t pte, unsigned long virt)
{
	if (*pte & AMD_IOMMU_PTE_PG_MODE_MASK)
		return INVALID_PHYS_ADDR;
	return (*pte & BIT_MASK(51, 21)) | (virt & BIT_MASK(20, 0));
}

int vcpu_vendor_early_init(void)
{
	unsigned long vm_cr;
	int err;

	err = svm_check_features();
	if (err)
		return err;

	vm_cr = read_msr(MSR_VM_CR);
	if (vm_cr & VM_CR_SVMDIS)
		/* SVM disabled in BIOS */
		return trace_error(-EPERM);

	/*
	 * Nested paging is almost the same as the native one. However, we
	 * need to override some handlers in order to reuse the page table for
	 * the IOMMU as well.
	 */
	memcpy(npt_iommu_paging, x86_64_paging, sizeof(npt_iommu_paging));
	npt_iommu_paging[0].set_next_pt = npt_iommu_set_next_pt_l4;
	npt_iommu_paging[1].set_next_pt = npt_iommu_set_next_pt_l3;
	npt_iommu_paging[2].set_next_pt = npt_iommu_set_next_pt_l2;
	npt_iommu_paging[1].get_phys = npt_iommu_get_phys_l3;
	npt_iommu_paging[2].get_phys = npt_iommu_get_phys_l2;

	parking_pt.root_paging = npt_iommu_paging;

	/* This is always false for AMD now (except in nested SVM);
	   see Sect. 16.3.1 in APMv2 */
	if (using_x2apic) {
		/* allow direct x2APIC access except for ICR writes */
		memset(&msrpm[SVM_MSRPM_0000][MSR_X2APIC_BASE/4], 0,
				(MSR_X2APIC_END - MSR_X2APIC_BASE + 1)/4);
		msrpm[SVM_MSRPM_0000][MSR_X2APIC_ICR/4] = 0x02;
	} else {
		if (has_avic) {
			avic_page = page_alloc(&remap_pool, 1);
			if (!avic_page)
				return trace_error(-ENOMEM);
		}
	}

	return vcpu_cell_init(&root_cell);
}

int vcpu_vendor_cell_init(struct cell *cell)
{
	int err = -ENOMEM;
	u64 flags;

	/* allocate iopm  */
	cell->arch.svm.iopm = page_alloc(&mem_pool, IOPM_PAGES);
	if (!cell->arch.svm.iopm)
		return err;

	/* build root NPT of cell */
	cell->arch.svm.npt_iommu_structs.root_paging = npt_iommu_paging;
	cell->arch.svm.npt_iommu_structs.root_table =
		(page_table_t)cell->arch.root_table_page;

	if (!has_avic) {
		/*
		 * Map xAPIC as is; reads are passed, writes are trapped.
		 */
		flags = PAGE_READONLY_FLAGS | PAGE_FLAG_US | PAGE_FLAG_DEVICE;
		err = paging_create(&cell->arch.svm.npt_iommu_structs,
				    XAPIC_BASE, PAGE_SIZE, XAPIC_BASE,
				    flags, PAGING_NON_COHERENT);
	} else {
		flags = PAGE_DEFAULT_FLAGS | PAGE_FLAG_DEVICE;
		err = paging_create(&cell->arch.svm.npt_iommu_structs,
				    paging_hvirt2phys(avic_page),
				    PAGE_SIZE, XAPIC_BASE,
				    flags, PAGING_NON_COHERENT);
	}
	if (err)
		goto err_free_iopm;

	return 0;

err_free_iopm:
	page_free(&mem_pool, cell->arch.svm.iopm, 3);

	return err;
}

int vcpu_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	u64 phys_start = mem->phys_start;
	u64 flags = PAGE_FLAG_US; /* See APMv2, Section 15.25.5 */

	if (mem->flags & JAILHOUSE_MEM_READ)
		flags |= PAGE_FLAG_PRESENT;
	if (mem->flags & JAILHOUSE_MEM_WRITE)
		flags |= PAGE_FLAG_RW;
	if (!(mem->flags & JAILHOUSE_MEM_EXECUTE))
		flags |= PAGE_FLAG_NOEXECUTE;
	if (mem->flags & JAILHOUSE_MEM_COMM_REGION)
		phys_start = paging_hvirt2phys(&cell->comm_page);

	flags |= amd_iommu_get_memory_region_flags(mem);

	/*
	 * As we also manipulate the IOMMU page table, changes need to be
	 * coherent.
	 */
	return paging_create(&cell->arch.svm.npt_iommu_structs, phys_start,
			     mem->size, mem->virt_start, flags,
			     PAGING_COHERENT);
}

int vcpu_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	return paging_destroy(&cell->arch.svm.npt_iommu_structs,
			      mem->virt_start, mem->size, PAGING_COHERENT);
}

void vcpu_vendor_cell_exit(struct cell *cell)
{
	paging_destroy(&cell->arch.svm.npt_iommu_structs, XAPIC_BASE,
		       PAGE_SIZE, PAGING_NON_COHERENT);
	page_free(&mem_pool, cell->arch.svm.iopm, 3);
}

int vcpu_init(struct per_cpu *cpu_data)
{
	unsigned long efer;
	int err;

	err = svm_check_features();
	if (err)
		return err;

	efer = read_msr(MSR_EFER);
	if (efer & EFER_SVME)
		return trace_error(-EBUSY);

	efer |= EFER_SVME;
	write_msr(MSR_EFER, efer);

	cpu_data->svm_state = SVMON;

	vmcb_setup(cpu_data);

	/*
	 * APM Volume 2, 3.1.1: "When writing the CR0 register, software should
	 * set the values of reserved bits to the values found during the
	 * previous CR0 read."
	 * But we want to avoid surprises with new features unknown to us but
	 * set by Linux. So check if any assumed revered bit was set and bail
	 * out if so.
	 * Note that the APM defines all reserved CR4 bits as must-be-zero.
	 */
	if (cpu_data->linux_cr0 & X86_CR0_RESERVED)
		return trace_error(-EIO);

	/* bring CR0 and CR4 into well-defined states */
	write_cr0(X86_CR0_HOST_STATE);
	write_cr4(X86_CR4_HOST_STATE);

	write_msr(MSR_VM_HSAVE_PA, paging_hvirt2phys(cpu_data->host_state));

	return 0;
}

void vcpu_exit(struct per_cpu *cpu_data)
{
	unsigned long efer;

	if (cpu_data->svm_state == SVMOFF)
		return;

	cpu_data->svm_state = SVMOFF;

	/* We are leaving - set the GIF */
	asm volatile ("stgi" : : : "memory");

	efer = read_msr(MSR_EFER);
	efer &= ~EFER_SVME;
	write_msr(MSR_EFER, efer);

	write_msr(MSR_VM_HSAVE_PA, 0);
}

void __attribute__((noreturn)) vcpu_activate_vmm(void)
{
	struct per_cpu *cpu_data = this_cpu_data();
	unsigned long vmcb_pa, host_stack;

	vmcb_pa = paging_hvirt2phys(&cpu_data->vmcb);
	host_stack = (unsigned long)cpu_data->stack + sizeof(cpu_data->stack);

	/* We enter Linux at the point arch_entry would return to as well.
	 * rax is cleared to signal success to the caller. */
	asm volatile(
		"clgi\n\t"
		"mov (%%rdi),%%r15\n\t"
		"mov 0x8(%%rdi),%%r14\n\t"
		"mov 0x10(%%rdi),%%r13\n\t"
		"mov 0x18(%%rdi),%%r12\n\t"
		"mov 0x20(%%rdi),%%rbx\n\t"
		"mov 0x28(%%rdi),%%rbp\n\t"
		"mov %2,%%rsp\n\t"
		"vmload %%rax\n\t"
		"jmp svm_vmentry"
		: /* no output */
		: "D" (cpu_data->linux_reg), "a" (vmcb_pa), "m" (host_stack));
	__builtin_unreachable();
}

void __attribute__((noreturn)) vcpu_deactivate_vmm(void)
{
	struct per_cpu *cpu_data = this_cpu_data();
	struct vmcb *vmcb = &cpu_data->vmcb;
	unsigned long *stack = (unsigned long *)vmcb->rsp;
	unsigned long linux_ip = vmcb->rip;

	cpu_data->linux_cr0 = vmcb->cr0;
	cpu_data->linux_cr3 = vmcb->cr3;

	cpu_data->linux_gdtr.base = vmcb->gdtr.base;
	cpu_data->linux_gdtr.limit = vmcb->gdtr.limit;
	cpu_data->linux_idtr.base = vmcb->idtr.base;
	cpu_data->linux_idtr.limit = vmcb->idtr.limit;

	cpu_data->linux_cs.selector = vmcb->cs.selector;

	asm volatile("str %0" : "=m" (cpu_data->linux_tss.selector));

	cpu_data->linux_efer = vmcb->efer & (~EFER_SVME);
	cpu_data->linux_fs.base = read_msr(MSR_FS_BASE);
	cpu_data->linux_gs.base = vmcb->gs.base;

	cpu_data->linux_ds.selector = vmcb->ds.selector;
	cpu_data->linux_es.selector = vmcb->es.selector;

	asm volatile("mov %%fs,%0" : "=m" (cpu_data->linux_fs.selector));
	asm volatile("mov %%gs,%0" : "=m" (cpu_data->linux_gs.selector));

	arch_cpu_restore(cpu_data, 0);

	stack--;
	*stack = linux_ip;

	asm volatile (
		"mov %%rbx,%%rsp\n\t"
		"pop %%r15\n\t"
		"pop %%r14\n\t"
		"pop %%r13\n\t"
		"pop %%r12\n\t"
		"pop %%r11\n\t"
		"pop %%r10\n\t"
		"pop %%r9\n\t"
		"pop %%r8\n\t"
		"pop %%rdi\n\t"
		"pop %%rsi\n\t"
		"pop %%rbp\n\t"
		"add $8,%%rsp\n\t"
		"pop %%rbx\n\t"
		"pop %%rdx\n\t"
		"pop %%rcx\n\t"
		"mov %%rax,%%rsp\n\t"
		"xor %%rax,%%rax\n\t"
		"ret"
		: : "a" (stack), "b" (&cpu_data->guest_regs));
	__builtin_unreachable();
}

void vcpu_vendor_reset(unsigned int sipi_vector)
{
	static const struct svm_segment dataseg_reset_state = {
		.selector = 0,
		.base = 0,
		.limit = 0xffff,
		.attributes = 0x0093,
	};
	static const struct svm_segment dtr_reset_state = {
		.selector = 0,
		.base = 0,
		.limit = 0xffff,
		.attributes = 0,
	};
	struct per_cpu *cpu_data = this_cpu_data();
	struct vmcb *vmcb = &cpu_data->vmcb;
	unsigned long reset_addr;

	vmcb->cr0 = X86_CR0_NW | X86_CR0_CD | X86_CR0_ET;
	vmcb->cr3 = 0;
	vmcb->cr4 = 0;

	vmcb->rflags = 0x02;
	vmcb->rsp = 0;

	if (sipi_vector == APIC_BSP_PSEUDO_SIPI) {
		reset_addr = this_cell()->config->cpu_reset_address;

		vmcb->rip = reset_addr & 0xffff;

		vmcb->cs.selector = (reset_addr >> 4) & 0xf000;
		vmcb->cs.base = reset_addr & ~0xffffL;
	} else {
		vmcb->rip = 0;

		vmcb->cs.selector = sipi_vector << 8;
		vmcb->cs.base = sipi_vector << 12;
	}

	vmcb->cs.limit = 0xffff;
	vmcb->cs.attributes = 0x009b;

	vmcb->ds = dataseg_reset_state;
	vmcb->es = dataseg_reset_state;
	vmcb->fs = dataseg_reset_state;
	vmcb->gs = dataseg_reset_state;
	vmcb->ss = dataseg_reset_state;

	vmcb->tr.selector = 0;
	vmcb->tr.base = 0;
	vmcb->tr.limit = 0xffff;
	vmcb->tr.attributes = 0x008b;

	vmcb->ldtr.selector = 0;
	vmcb->ldtr.base = 0;
	vmcb->ldtr.limit = 0xffff;
	vmcb->ldtr.attributes = 0x0082;

	vmcb->gdtr = dtr_reset_state;
	vmcb->idtr = dtr_reset_state;

	vmcb->efer = EFER_SVME;

	/* These MSRs are undefined on reset */
	vmcb->star = 0;
	vmcb->lstar = 0;
	vmcb->cstar = 0;
	vmcb->sfmask = 0;
	vmcb->sysenter_cs = 0;
	vmcb->sysenter_eip = 0;
	vmcb->sysenter_esp = 0;
	vmcb->kerngsbase = 0;

	vmcb->dr7 = 0x00000400;

	vmcb->eventinj = 0;

	/* Almost all of the guest state changed */
	vmcb->clean_bits = 0;

	svm_set_cell_config(cpu_data->cell, vmcb);

	asm volatile(
		"vmload %%rax"
		: : "a" (paging_hvirt2phys(vmcb)) : "memory");
	/* vmload overwrites GS_BASE - restore the host state */
	write_msr(MSR_GS_BASE, (unsigned long)cpu_data);
}

void vcpu_skip_emulated_instruction(unsigned int inst_len)
{
	this_cpu_data()->vmcb.rip += inst_len;
}

static void update_efer(struct vmcb *vmcb)
{
	unsigned long efer = vmcb->efer;

	if ((efer & (EFER_LME | EFER_LMA)) != EFER_LME)
		return;

	efer |= EFER_LMA;

	/* Flush TLB on LMA/LME change: See APMv2, Sect. 15.16 */
	if ((vmcb->efer ^ efer) & EFER_LMA)
		vcpu_tlb_flush();

	vmcb->efer = efer;
	vmcb->clean_bits &= ~CLEAN_BITS_CRX;
}

void vcpu_get_guest_paging_structs(struct guest_paging_structures *pg_structs)
{
	struct vmcb *vmcb = &this_cpu_data()->vmcb;

	if (vmcb->efer & EFER_LMA) {
		pg_structs->root_paging = x86_64_paging;
		pg_structs->root_table_gphys = vmcb->cr3 & BIT_MASK(51, 12);
	} else if (!(vmcb->cr0 & X86_CR0_PG)) {
		pg_structs->root_paging = NULL;
	} else if (vmcb->cr4 & X86_CR4_PAE) {
		pg_structs->root_paging = pae_paging;
		pg_structs->root_table_gphys = vmcb->cr3 & BIT_MASK(31, 5);
	} else {
		pg_structs->root_paging = i386_paging;
		pg_structs->root_table_gphys = vmcb->cr3 & BIT_MASK(31, 12);
	}
}

void vcpu_vendor_set_guest_pat(unsigned long val)
{
	struct vmcb *vmcb = &this_cpu_data()->vmcb;

	vmcb->g_pat = val;
	vmcb->clean_bits &= ~CLEAN_BITS_NP;
}

struct parse_context {
	unsigned int remaining;
	unsigned int size;
	unsigned long cs_base;
	const u8 *inst;
};

static bool ctx_advance(struct parse_context *ctx,
			unsigned long *pc,
			struct guest_paging_structures *pg_structs)
{
	if (!ctx->size) {
		ctx->size = ctx->remaining;
		ctx->inst = vcpu_map_inst(pg_structs, ctx->cs_base + *pc,
					  &ctx->size);
		if (!ctx->inst)
			return false;
		ctx->remaining -= ctx->size;
		*pc += ctx->size;
	}
	return true;
}

static bool svm_parse_mov_to_cr(struct vmcb *vmcb, unsigned long pc,
				unsigned char reg, unsigned long *gpr)
{
	struct guest_paging_structures pg_structs;
	struct parse_context ctx = {};
	/* No prefixes are supported yet */
	u8 opcodes[] = {0x0f, 0x22}, modrm;
	int n;

	vcpu_get_guest_paging_structs(&pg_structs);

	ctx.remaining = ARRAY_SIZE(opcodes);
	ctx.cs_base = (vmcb->efer & EFER_LMA) ? 0 : vmcb->cs.base;

	if (!ctx_advance(&ctx, &pc, &pg_structs))
		return false;

	for (n = 0; n < ARRAY_SIZE(opcodes); n++, ctx.inst++)
		if (*(ctx.inst) != opcodes[n] ||
		    !ctx_advance(&ctx, &pc, &pg_structs))
			return false;

	if (!ctx_advance(&ctx, &pc, &pg_structs))
		return false;

	modrm = *(ctx.inst);

	if (((modrm & 0x38) >> 3) != reg)
		return false;

	if (gpr)
		*gpr = (modrm & 0x7);

	return true;
}

/*
 * XXX: The only visible reason to have this function (vmx.c consistency
 * aside) is to prevent cells from setting invalid CD+NW combinations that
 * result in no more than VMEXIT_INVALID. Maybe we can get along without it
 * altogether?
 */
static bool svm_handle_cr(struct per_cpu *cpu_data)
{
	struct vmcb *vmcb = &cpu_data->vmcb;
	/* Workaround GCC 4.8 warning on uninitialized variable 'reg' */
	unsigned long reg = -1, val, bits;

	if (has_assists) {
		if (!(vmcb->exitinfo1 & (1UL << 63))) {
			panic_printk("FATAL: Unsupported CR access (LMSW or CLTS)\n");
			return false;
		}
		reg = vmcb->exitinfo1 & 0x07;
	} else {
		if (!svm_parse_mov_to_cr(vmcb, vmcb->rip, 0, &reg)) {
			panic_printk("FATAL: Unable to parse MOV-to-CR instruction\n");
			return false;
		}
	}

	if (reg == 4)
		val = vmcb->rsp;
	else
		val = cpu_data->guest_regs.by_index[15 - reg];

	vcpu_skip_emulated_instruction(X86_INST_LEN_MOV_TO_CR);
	/* Flush TLB on PG/WP/CD/NW change: See APMv2, Sect. 15.16 */
	bits = (X86_CR0_PG | X86_CR0_WP | X86_CR0_CD | X86_CR0_NW);
	if ((val ^ vmcb->cr0) & bits)
		vcpu_tlb_flush();
	/* TODO: better check for #GP reasons */
	vmcb->cr0 = val & SVM_CR0_ALLOWED_BITS;
	if (val & X86_CR0_PG)
		update_efer(vmcb);
	vmcb->clean_bits &= ~CLEAN_BITS_CRX;

	return true;
}

static bool svm_handle_msr_write(struct per_cpu *cpu_data)
{
	struct vmcb *vmcb = &cpu_data->vmcb;
	unsigned long efer;

	if (cpu_data->guest_regs.rcx == MSR_EFER) {
		/* Never let a guest to disable SVME; see APMv2, Sect. 3.1.7 */
		efer = get_wrmsr_value(&cpu_data->guest_regs) | EFER_SVME;
		/* Flush TLB on LME/NXE change: See APMv2, Sect. 15.16 */
		if ((efer ^ vmcb->efer) & (EFER_LME | EFER_NXE))
			vcpu_tlb_flush();
		vmcb->efer = efer;
		vmcb->clean_bits &= ~CLEAN_BITS_CRX;
		vcpu_skip_emulated_instruction(X86_INST_LEN_WRMSR);
		return true;
	}

	return vcpu_handle_msr_write();
}

/*
 * TODO: This handles unaccelerated (non-AVIC) access. AVIC should
 * be treated separately in svm_handle_avic_access().
 */
static bool svm_handle_apic_access(struct vmcb *vmcb)
{
	struct guest_paging_structures pg_structs;
	unsigned int inst_len, offset;
	bool is_write;

	/* The caller is responsible for sanity checks */
	is_write = !!(vmcb->exitinfo1 & 0x2);
	offset = vmcb->exitinfo2 - XAPIC_BASE;

	if (offset & 0x00f)
		goto out_err;

	vcpu_get_guest_paging_structs(&pg_structs);

	inst_len = apic_mmio_access(&pg_structs, offset >> 4, is_write);
	if (!inst_len)
		goto out_err;

	vcpu_skip_emulated_instruction(inst_len);
	return true;

out_err:
	panic_printk("FATAL: Unhandled APIC access, offset %d, is_write: %d\n",
		     offset, is_write);
	return false;
}

static void dump_guest_regs(union registers *guest_regs, struct vmcb *vmcb)
{
	panic_printk("RIP: 0x%016llx RSP: 0x%016llx FLAGS: %llx\n", vmcb->rip,
		     vmcb->rsp, vmcb->rflags);
	panic_printk("RAX: 0x%016lx RBX: 0x%016lx RCX: 0x%016lx\n",
		     guest_regs->rax, guest_regs->rbx, guest_regs->rcx);
	panic_printk("RDX: 0x%016lx RSI: 0x%016lx RDI: 0x%016lx\n",
		     guest_regs->rdx, guest_regs->rsi, guest_regs->rdi);
	panic_printk("CS: %x BASE: 0x%016llx AR-BYTES: %x EFER.LMA %d\n",
		     vmcb->cs.selector, vmcb->cs.base, vmcb->cs.attributes,
		     !!(vmcb->efer & EFER_LMA));
	panic_printk("CR0: 0x%016llx CR3: 0x%016llx CR4: 0x%016llx\n",
		     vmcb->cr0, vmcb->cr3, vmcb->cr4);
	panic_printk("EFER: 0x%016llx\n", vmcb->efer);
}

void vcpu_vendor_get_io_intercept(struct vcpu_io_intercept *io)
{
	struct vmcb *vmcb = &this_cpu_data()->vmcb;
	u64 exitinfo = vmcb->exitinfo1;

	/* parse exit info for I/O instructions (see APM, 15.10.2 ) */
	io->port = (exitinfo >> 16) & 0xFFFF;
	io->size = (exitinfo >> 4) & 0x7;
	io->in = !!(exitinfo & 0x1);
	io->inst_len = vmcb->exitinfo2 - vmcb->rip;
	io->rep_or_str = !!(exitinfo & 0x0c);
}

void vcpu_vendor_get_mmio_intercept(struct vcpu_mmio_intercept *mmio)
{
	struct vmcb *vmcb = &this_cpu_data()->vmcb;

	mmio->phys_addr = vmcb->exitinfo2;
	mmio->is_write = !!(vmcb->exitinfo1 & 0x2);
}

unsigned long vcpu_vendor_get_guest_cr4(void)
{
	return this_cpu_data()->vmcb.cr4;
}

void vcpu_handle_exit(struct per_cpu *cpu_data)
{
	struct vmcb *vmcb = &cpu_data->vmcb;
	bool res = false;

	vmcb->gs.base = read_msr(MSR_GS_BASE);

	/* Restore GS value expected by per_cpu data accessors */
	write_msr(MSR_GS_BASE, (unsigned long)cpu_data);

	cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_TOTAL]++;
	/*
	 * All guest state is marked unmodified; individual handlers must clear
	 * the bits as needed.
	 */
	vmcb->clean_bits = 0xffffffff;

	switch (vmcb->exitcode) {
	case VMEXIT_INVALID:
		panic_printk("FATAL: VM-Entry failure, error %lld\n",
			     vmcb->exitcode);
		break;
	case VMEXIT_NMI:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_MANAGEMENT]++;
		/* Temporarily enable GIF to consume pending NMI */
		asm volatile("stgi; clgi" : : : "memory");
		x86_check_events();
		goto vmentry;
	case VMEXIT_VMMCALL:
		vcpu_handle_hypercall();
		goto vmentry;
	case VMEXIT_CR0_SEL_WRITE:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_CR]++;
		if (svm_handle_cr(cpu_data))
			goto vmentry;
		break;
	case VMEXIT_CPUID:
		vcpu_handle_cpuid();
		goto vmentry;
	case VMEXIT_MSR:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_MSR]++;
		if (!vmcb->exitinfo1)
			res = vcpu_handle_msr_read();
		else
			res = svm_handle_msr_write(cpu_data);
		if (res)
			goto vmentry;
		break;
	case VMEXIT_NPF:
		if ((vmcb->exitinfo1 & 0x7) == 0x7 &&
		     vmcb->exitinfo2 >= XAPIC_BASE &&
		     vmcb->exitinfo2 < XAPIC_BASE + PAGE_SIZE) {
			/* APIC access in non-AVIC mode */
			cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_XAPIC]++;
			if (svm_handle_apic_access(vmcb))
				goto vmentry;
		} else {
			/* General MMIO (IOAPIC, PCI etc) */
			cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_MMIO]++;
			if (vcpu_handle_mmio_access())
				goto vmentry;
		}
		break;
	case VMEXIT_IOIO:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_PIO]++;
		if (vcpu_handle_io_access())
			goto vmentry;
		break;
	case VMEXIT_EXCEPTION_DB:
	case VMEXIT_EXCEPTION_AC:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_EXCEPTION]++;
		/* Reinject exception, including error code if needed. */
		vmcb->eventinj = (vmcb->exitcode - VMEXIT_EXCEPTION_DE) |
			SVM_EVENTINJ_EXCEPTION | SVM_EVENTINJ_VALID;
		if (vmcb->exitcode == VMEXIT_EXCEPTION_AC) {
			vmcb->eventinj |= SVM_EVENTINJ_ERR_VALID;
			vmcb->eventinj_err = vmcb->exitinfo1;
		}
		x86_check_events();
		goto vmentry;
	/* TODO: Handle VMEXIT_AVIC_NOACCEL and VMEXIT_AVIC_INCOMPLETE_IPI */
	default:
		panic_printk("FATAL: Unexpected #VMEXIT, exitcode %llx, "
			     "exitinfo1 0x%016llx exitinfo2 0x%016llx\n",
			     vmcb->exitcode, vmcb->exitinfo1, vmcb->exitinfo2);
	}
	dump_guest_regs(&cpu_data->guest_regs, vmcb);
	panic_park();

vmentry:
	write_msr(MSR_GS_BASE, vmcb->gs.base);
}

void vcpu_park(void)
{
#ifdef CONFIG_CRASH_CELL_ON_PANIC
	if (this_cpu_data()->failed) {
		this_cpu_data()->vmcb.rip = 0;
		return;
	}
#endif
	vcpu_vendor_reset(0);
	/* No need to clear VMCB Clean bit: vcpu_vendor_reset() already does
	 * this. */
	this_cpu_data()->vmcb.n_cr3 = paging_hvirt2phys(parking_pt.root_table);

	vcpu_tlb_flush();
}

void vcpu_nmi_handler(void)
{
}

void vcpu_tlb_flush(void)
{
	struct vmcb *vmcb = &this_cpu_data()->vmcb;

	if (has_flush_by_asid)
		vmcb->tlb_control = SVM_TLB_FLUSH_GUEST;
	else
		vmcb->tlb_control = SVM_TLB_FLUSH_ALL;
}

const u8 *vcpu_get_inst_bytes(const struct guest_paging_structures *pg_structs,
			      unsigned long pc, unsigned int *size)
{
	struct vmcb *vmcb = &this_cpu_data()->vmcb;
	unsigned long start;

	if (has_assists) {
		if (!*size)
			return NULL;
		start = vmcb->rip - pc;
		if (start < vmcb->bytes_fetched) {
			*size = vmcb->bytes_fetched - start;
			return &vmcb->guest_bytes[start];
		} else {
			return NULL;
		}
	} else {
		return vcpu_map_inst(pg_structs, pc, size);
	}
}

void vcpu_vendor_get_cell_io_bitmap(struct cell *cell,
		                    struct vcpu_io_bitmap *iobm)
{
	iobm->data = cell->arch.svm.iopm;
	iobm->size = IOPM_PAGES * PAGE_SIZE;
}

#define VCPU_VENDOR_GET_REGISTER(__reg__)	\
u64 vcpu_vendor_get_##__reg__(void)		\
{						\
	return this_cpu_data()->vmcb.__reg__;	\
}

VCPU_VENDOR_GET_REGISTER(efer);
VCPU_VENDOR_GET_REGISTER(rflags);
VCPU_VENDOR_GET_REGISTER(rip);

u16 vcpu_vendor_get_cs_attr(void)
{
	/*
	 * Build the CS segment attributes from the L and D/B extracted from
	 * the segment attribute field and the CPL from its own field. The
	 * latter is suggested by the AMD spec (Vol 2, 15.5.1). Present the
	 * result in Intel format.
	 */
	u16 l_db = this_cpu_data()->vmcb.cs.attributes & BIT_MASK(10, 9);

	return (this_cpu_data()->vmcb.cpl << 5) | (l_db << 4);
}

/* GIF must be set for interrupts to be delivered (APMv2, Sect. 15.17) */
void enable_irq(void)
{
	asm volatile("stgi; sti" : : : "memory");
}

/* Jailhouse runs with GIF cleared, so we need to restore this state */
void disable_irq(void)
{
	asm volatile("cli; clgi" : : : "memory");
}
