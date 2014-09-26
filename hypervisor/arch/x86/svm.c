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
#include <jailhouse/cell-config.h>
#include <jailhouse/paging.h>
#include <jailhouse/processor.h>
#include <jailhouse/string.h>
#include <asm/apic.h>
#include <asm/cell.h>
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
#define SVM_CR0_CLEARED_BITS	~X86_CR0_NW

static bool has_avic, has_assists, has_flush_by_asid;

static const struct segment invalid_seg;

static struct paging npt_paging[NPT_PAGE_DIR_LEVELS];

static u8 __attribute__((aligned(PAGE_SIZE))) msrpm[][0x2000/4] = {
	[ SVM_MSRPM_0000 ] = {
		[      0/4 ...  0x017/4 ] = 0,
		[  0x018/4 ...  0x01b/4 ] = 0x80, /* 0x01b (w) */
		[  0x01c/4 ...  0x7ff/4 ] = 0,
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
	if (!(cpuid_ecx(0x80000001) & X86_FEATURE_SVM))
		return -ENODEV;

	/* Nested paging */
	if (!(cpuid_edx(0x8000000A) & X86_FEATURE_NP))
		return -EIO;

	/* Decode assists */
	if ((cpuid_edx(0x8000000A) & X86_FEATURE_DECODE_ASSISTS))
		has_assists = true;

	/* AVIC support */
	if (cpuid_edx(0x8000000A) & X86_FEATURE_AVIC)
		has_avic = true;

	/* TLB Flush by ASID support */
	if (cpuid_edx(0x8000000A) & X86_FEATURE_FLUSH_BY_ASID)
		has_flush_by_asid = true;

	return 0;
}

static void set_svm_segment_from_dtr(struct svm_segment *svm_segment,
				     const struct desc_table_reg *dtr)
{
	struct svm_segment tmp = { 0 };

	if (dtr) {
		tmp.base = dtr->base;
		tmp.limit = dtr->limit & 0xffff;
	}

	*svm_segment = tmp;
}

/* TODO: struct segment needs to be x86 generic, not VMX-specific one here */
static void set_svm_segment_from_segment(struct svm_segment *svm_segment,
					 const struct segment *segment)
{
	u32 ar;

	svm_segment->selector = segment->selector;

	if (segment->access_rights == 0x10000) {
		svm_segment->access_rights = 0;
	} else {
		ar = segment->access_rights;
		svm_segment->access_rights =
			((ar & 0xf000) >> 4) | (ar & 0x00ff);
	}

	svm_segment->limit = segment->limit;
	svm_segment->base = segment->base;
}

static bool vcpu_set_cell_config(struct cell *cell, struct vmcb *vmcb)
{
	/* No real need for this function; used for consistency with vmx.c */
	vmcb->iopm_base_pa = paging_hvirt2phys(cell->svm.iopm);
	vmcb->n_cr3 = paging_hvirt2phys(cell->svm.npt_structs.root_table);

	return true;
}

static int vmcb_setup(struct per_cpu *cpu_data)
{
	struct vmcb *vmcb = &cpu_data->vmcb;

	memset(vmcb, 0, sizeof(struct vmcb));

	vmcb->cr0 = read_cr0() & SVM_CR0_CLEARED_BITS;
	vmcb->cr3 = cpu_data->linux_cr3;
	vmcb->cr4 = read_cr4();

	set_svm_segment_from_segment(&vmcb->cs, &cpu_data->linux_cs);
	set_svm_segment_from_segment(&vmcb->ds, &cpu_data->linux_ds);
	set_svm_segment_from_segment(&vmcb->es, &cpu_data->linux_es);
	set_svm_segment_from_segment(&vmcb->fs, &cpu_data->linux_fs);
	set_svm_segment_from_segment(&vmcb->gs, &cpu_data->linux_gs);
	set_svm_segment_from_segment(&vmcb->ss, &invalid_seg);
	set_svm_segment_from_segment(&vmcb->tr, &cpu_data->linux_tss);

	set_svm_segment_from_dtr(&vmcb->ldtr, NULL);
	set_svm_segment_from_dtr(&vmcb->gdtr, &cpu_data->linux_gdtr);
	set_svm_segment_from_dtr(&vmcb->idtr, &cpu_data->linux_idtr);

	vmcb->cpl = 0; /* Linux runs in ring 0 before migration */

	vmcb->rflags = 0x02;
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

	/* Linux uses custom PAT setting */
	vmcb->g_pat = read_msr(MSR_IA32_PAT);

	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_NMI;
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_CR0_SEL_WRITE;
	/* TODO: Do we need this for SVM ? */
	/* vmcb->general1_intercepts |= GENERAL1_INTERCEPT_CPUID; */
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_IOIO_PROT;
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_MSR_PROT;
	vmcb->general1_intercepts |= GENERAL1_INTERCEPT_SHUTDOWN_EVT;

	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_VMRUN; /* Required */
	vmcb->general2_intercepts |= GENERAL2_INTERCEPT_VMMCALL;

	vmcb->msrpm_base_pa = paging_hvirt2phys(msrpm);

	vmcb->np_enable = 1;
	/* No more than one guest owns the CPU */
	vmcb->guest_asid = 1;

	/* TODO: Setup AVIC */

	return vcpu_set_cell_config(cpu_data->cell, vmcb);
}

unsigned long arch_paging_gphys2phys(struct per_cpu *cpu_data,
				     unsigned long gphys,
				     unsigned long flags)
{
	/* TODO: Implement */
	return INVALID_PHYS_ADDR;
}

static void npt_set_next_pt(pt_entry_t pte, unsigned long next_pt)
{
	/* See APMv2, Section 15.25.5 */
	*pte = (next_pt & 0x000ffffffffff000UL) |
		(PAGE_DEFAULT_FLAGS | PAGE_FLAG_US);
}

int vcpu_vendor_init(void)
{
	unsigned long vm_cr;
	int err, n;

	err = svm_check_features();
	if (err)
		return err;

	vm_cr = read_msr(MSR_VM_CR);
	if (vm_cr & VM_CR_SVMDIS)
		/* SVM disabled in BIOS */
		return -EPERM;

	/* Nested paging is the same as the native one */
	memcpy(npt_paging, x86_64_paging, sizeof(npt_paging));
	for (n = 0; n < NPT_PAGE_DIR_LEVELS; n++)
		npt_paging[n].set_next_pt = npt_set_next_pt;

	/* This is always false for AMD now (except in nested SVM);
	   see Sect. 16.3.1 in APMv2 */
	if (using_x2apic) {
		/* allow direct x2APIC access except for ICR writes */
		memset(&msrpm[SVM_MSRPM_0000][MSR_X2APIC_BASE/4], 0,
				(MSR_X2APIC_END - MSR_X2APIC_BASE + 1)/4);
		msrpm[SVM_MSRPM_0000][MSR_X2APIC_ICR/4] = 0x02;
	} else {
		/* Enable Extended Interrupt LVT */
		apic_reserved_bits[0x50] = 0;
		if (has_avic) {
			avic_page = page_alloc(&remap_pool, 1);
			if (!avic_page)
				return -ENOMEM;
		}
	}

	return vcpu_cell_init(&root_cell);
}

int vcpu_vendor_cell_init(struct cell *cell)
{
	/* TODO: Implement */
	return 0;
}

int vcpu_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	/* TODO: Implement */
	return 0;
}

int vcpu_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	/* TODO: Implement */
	return 0;
}

void vcpu_vendor_cell_exit(struct cell *cell)
{
	/* TODO: Implement */
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
		return -EBUSY;

	efer |= EFER_SVME;
	write_msr(MSR_EFER, efer);

	cpu_data->svm_state = SVMON;

	if (!vmcb_setup(cpu_data))
		return -EIO;

	write_msr(MSR_VM_HSAVE_PA, paging_hvirt2phys(cpu_data->host_state));

	/* Enable Extended Interrupt LVT (xAPIC, as it is AMD-only) */
	if (!using_x2apic)
		apic_reserved_bits[0x50] = 0;

	return 0;
}

void vcpu_exit(struct per_cpu *cpu_data)
{
	unsigned long efer;

	if (cpu_data->svm_state == SVMOFF)
		return;

	cpu_data->svm_state = SVMOFF;

	efer = read_msr(MSR_EFER);
	efer &= ~EFER_SVME;
	write_msr(MSR_EFER, efer);

	write_msr(MSR_VM_HSAVE_PA, 0);
}

void vcpu_activate_vmm(struct per_cpu *cpu_data)
{
	/* TODO: Implement */
	__builtin_unreachable();
}

void __attribute__((noreturn))
vcpu_deactivate_vmm(struct registers *guest_regs)
{
	/* TODO: Implement */
	__builtin_unreachable();
}

void vcpu_skip_emulated_instruction(unsigned int inst_len)
{
	/* TODO: Implement */
}

bool vcpu_get_guest_paging_structs(struct guest_paging_structures *pg_structs)
{
	/* TODO: Implement */
	return false;
}

void vcpu_handle_exit(struct registers *guest_regs, struct per_cpu *cpu_data)
{
	/* TODO: Implement */
}

void vcpu_park(struct per_cpu *cpu_data)
{
	/* TODO: Implement */
}

void vcpu_nmi_handler(struct per_cpu *cpu_data)
{
	/* TODO: Implement */
}

void vcpu_tlb_flush(void)
{
	/* TODO: Implement */
}

const u8 *vcpu_get_inst_bytes(const struct guest_paging_structures *pg_structs,
			      unsigned long pc, unsigned int *size)
{
	/* TODO: Implement */
	return NULL;
}

void vcpu_vendor_get_cell_io_bitmap(struct cell *cell,
		                    struct vcpu_io_bitmap *out)
{
	/* TODO: Implement */
}

void vcpu_vendor_get_execution_state(struct vcpu_execution_state *x_state)
{
	/* TODO: Implement */
}
