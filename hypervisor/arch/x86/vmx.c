/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
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
#include <jailhouse/processor.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/control.h>
#include <jailhouse/hypercall.h>
#include <asm/apic.h>
#include <asm/control.h>
#include <asm/iommu.h>
#include <asm/vcpu.h>
#include <asm/vmx.h>

#define CR0_IDX			0
#define CR4_IDX			1

#define PIO_BITMAP_PAGES	2

static const struct segment invalid_seg = {
	.access_rights = 0x10000
};

/* bit cleared: direct access allowed */
// TODO: convert to whitelist
static u8 __attribute__((aligned(PAGE_SIZE))) msr_bitmap[][0x2000/8] = {
	[ VMX_MSR_BMP_0000_READ ] = {
		[      0/8 ...  0x26f/8 ] = 0,
		[  0x270/8 ...  0x277/8 ] = 0x80, /* 0x277 */
		[  0x278/8 ...  0x2f7/8 ] = 0,
		[  0x2f8/8 ...  0x2ff/8 ] = 0x80, /* 0x2ff */
		[  0x300/8 ...  0x7ff/8 ] = 0,
		[  0x800/8 ...  0x807/8 ] = 0x0c, /* 0x802, 0x803 */
		[  0x808/8 ...  0x80f/8 ] = 0xa5, /* 0x808, 0x80a, 0x80d, 0x80f */
		[  0x810/8 ...  0x817/8 ] = 0xff, /* 0x810 - 0x817 */
		[  0x818/8 ...  0x81f/8 ] = 0xff, /* 0x818 - 0x81f */
		[  0x820/8 ...  0x827/8 ] = 0xff, /* 0x820 - 0x827 */
		[  0x828/8 ...  0x82f/8 ] = 0x81, /* 0x828, 0x82f */
		[  0x830/8 ...  0x837/8 ] = 0xfd, /* 0x830, 0x832 - 0x837 */
		[  0x838/8 ...  0x83f/8 ] = 0x43, /* 0x838, 0x839, 0x83e */
		[  0x840/8 ... 0x1fff/8 ] = 0,
	},
	[ VMX_MSR_BMP_C000_READ ] = {
		[      0/8 ... 0x1fff/8 ] = 0,
	},
	[ VMX_MSR_BMP_0000_WRITE ] = {
		[      0/8 ...   0x17/8 ] = 0,
		[   0x18/8 ...   0x1f/8 ] = 0x08, /* 0x01b */
		[   0x20/8 ...  0x1ff/8 ] = 0,
		[  0x200/8 ...  0x277/8 ] = 0xff, /* 0x200 - 0x277 */
		[  0x278/8 ...  0x2f7/8 ] = 0,
		[  0x2f8/8 ...  0x2ff/8 ] = 0x80, /* 0x2ff */
		[  0x300/8 ...  0x387/8 ] = 0,
		[  0x388/8 ...  0x38f/8 ] = 0x80, /* 0x38f */
		[  0x390/8 ...  0x7ff/8 ] = 0,
		[  0x808/8 ...  0x80f/8 ] = 0x89, /* 0x808, 0x80b, 0x80f */
		[  0x810/8 ...  0x827/8 ] = 0,
		[  0x828/8 ...  0x82f/8 ] = 0x81, /* 0x828, 0x82f */
		[  0x830/8 ...  0x837/8 ] = 0xfd, /* 0x830, 0x832 - 0x837 */
		[  0x838/8 ...  0x83f/8 ] = 0xc1, /* 0x838, 0x83e, 0x83f */
		[  0x840/8 ...  0xd8f/8 ] = 0xff, /* esp. 0xc80 - 0xd8f */
		[  0xd90/8 ... 0x1fff/8 ] = 0,
	},
	[ VMX_MSR_BMP_C000_WRITE ] = {
		[      0/8 ... 0x1fff/8 ] = 0,
	},
};

/* Special access page to trap guest's attempts of accessing APIC in xAPIC mode */
static u8 __attribute__((aligned(PAGE_SIZE))) apic_access_page[PAGE_SIZE];
static struct paging ept_paging[EPT_PAGE_DIR_LEVELS];
static u32 secondary_exec_addon;
static unsigned long cr_maybe1[2], cr_required1[2];

static bool vmxon(void)
{
	unsigned long vmxon_addr;
	u8 ok;

	vmxon_addr = paging_hvirt2phys(&per_cpu(this_cpu_id())->vmxon_region);
	asm volatile(
		"vmxon (%1)\n\t"
		"seta %0"
		: "=rm" (ok)
		: "r" (&vmxon_addr), "m" (vmxon_addr)
		: "memory", "cc");
	return ok;
}

static bool vmcs_clear(void)
{
	unsigned long vmcs_addr;
	u8 ok;

	vmcs_addr = paging_hvirt2phys(&per_cpu(this_cpu_id())->vmcs);
	asm volatile(
		"vmclear (%1)\n\t"
		"seta %0"
		: "=qm" (ok)
		: "r" (&vmcs_addr), "m" (vmcs_addr)
		: "memory", "cc");
	return ok;
}

static bool vmcs_load(void)
{
	unsigned long vmcs_addr;
	u8 ok;

	vmcs_addr = paging_hvirt2phys(&per_cpu(this_cpu_id())->vmcs);
	asm volatile(
		"vmptrld (%1)\n\t"
		"seta %0"
		: "=qm" (ok)
		: "r" (&vmcs_addr), "m" (vmcs_addr)
		: "memory", "cc");
	return ok;
}

static inline unsigned long vmcs_read64(unsigned long field)
{
	unsigned long value;

	asm volatile("vmread %1,%0" : "=r" (value) : "r" (field) : "cc");
	return value;
}

static inline u16 vmcs_read16(unsigned long field)
{
	return vmcs_read64(field);
}

static inline u32 vmcs_read32(unsigned long field)
{
	return vmcs_read64(field);
}

static bool vmcs_write64(unsigned long field, unsigned long val)
{
	u8 ok;

	asm volatile(
		"vmwrite %1,%2\n\t"
		"setnz %0"
		: "=qm" (ok)
		: "r" (val), "r" (field)
		: "cc");
	if (!ok)
		printk("FATAL: vmwrite %08lx failed, error %d, caller %p\n",
		       field, vmcs_read32(VM_INSTRUCTION_ERROR),
		       __builtin_return_address(0));
	return ok;
}

static bool vmcs_write16(unsigned long field, u16 value)
{
	return vmcs_write64(field, value);
}

static bool vmcs_write32(unsigned long field, u32 value)
{
	return vmcs_write64(field, value);
}

static bool vmx_define_cr_restrictions(unsigned int cr_idx,
				       unsigned long maybe1,
				       unsigned long required1)
{
	if (!cr_maybe1[cr_idx]) {
		cr_maybe1[cr_idx] = maybe1;
		cr_required1[cr_idx] = required1;
		return true;
	}

	return cr_maybe1[cr_idx] == maybe1 &&
		cr_required1[cr_idx] == required1;
}

static int vmx_check_features(void)
{
	unsigned long vmx_proc_ctrl, vmx_proc_ctrl2, ept_cap;
	unsigned long vmx_pin_ctrl, vmx_basic, maybe1, required1;
	unsigned long vmx_entry_ctrl, vmx_exit_ctrl;

	if (!(cpuid_ecx(1, 0) & X86_FEATURE_VMX))
		return trace_error(-ENODEV);

	vmx_basic = read_msr(MSR_IA32_VMX_BASIC);

	/* require VMCS size <= PAGE_SIZE,
	 * VMCS memory access type == write back and
	 * availability of TRUE_*_CTLS */
	if (((vmx_basic >> 32) & 0x1fff) > PAGE_SIZE ||
	    ((vmx_basic >> 50) & 0xf) != EPT_TYPE_WRITEBACK ||
	    !(vmx_basic & (1UL << 55)))
		return trace_error(-EIO);

	/* require NMI exiting and preemption timer support */
	vmx_pin_ctrl = read_msr(MSR_IA32_VMX_PINBASED_CTLS) >> 32;
	if (!(vmx_pin_ctrl & PIN_BASED_NMI_EXITING) ||
	    !(vmx_pin_ctrl & PIN_BASED_VMX_PREEMPTION_TIMER))
		return trace_error(-EIO);

	/* require I/O and MSR bitmap as well as secondary controls support */
	vmx_proc_ctrl = read_msr(MSR_IA32_VMX_PROCBASED_CTLS) >> 32;
	if (!(vmx_proc_ctrl & CPU_BASED_USE_IO_BITMAPS) ||
	    !(vmx_proc_ctrl & CPU_BASED_USE_MSR_BITMAPS) ||
	    !(vmx_proc_ctrl & CPU_BASED_ACTIVATE_SECONDARY_CONTROLS))
		return trace_error(-EIO);

	/* require disabling of CR3 access interception */
	vmx_proc_ctrl = read_msr(MSR_IA32_VMX_TRUE_PROCBASED_CTLS);
	if (vmx_proc_ctrl &
	    (CPU_BASED_CR3_LOAD_EXITING | CPU_BASED_CR3_STORE_EXITING))
		return trace_error(-EIO);

	/* require APIC access, EPT and unrestricted guest mode support */
	vmx_proc_ctrl2 = read_msr(MSR_IA32_VMX_PROCBASED_CTLS2) >> 32;
	ept_cap = read_msr(MSR_IA32_VMX_EPT_VPID_CAP);
	if (!(vmx_proc_ctrl2 & SECONDARY_EXEC_VIRTUALIZE_APIC_ACCESSES) ||
	    !(vmx_proc_ctrl2 & SECONDARY_EXEC_ENABLE_EPT) ||
	    (ept_cap & EPT_MANDATORY_FEATURES) != EPT_MANDATORY_FEATURES ||
	    !(ept_cap & (EPT_INVEPT_SINGLE | EPT_INVEPT_GLOBAL)) ||
	    !(vmx_proc_ctrl2 & SECONDARY_EXEC_UNRESTRICTED_GUEST))
		return trace_error(-EIO);

	/* require RDTSCP, INVPCID, XSAVES if present in CPUID */
	if (cpuid_edx(0x80000001, 0) & X86_FEATURE_RDTSCP)
		secondary_exec_addon |= SECONDARY_EXEC_RDTSCP;
	if (cpuid_ebx(0x07, 0) & X86_FEATURE_INVPCID)
		secondary_exec_addon |= SECONDARY_EXEC_INVPCID;
	if (cpuid_eax(0x0d, 1) & X86_FEATURE_XSAVES)
		secondary_exec_addon |= SECONDARY_EXEC_XSAVES;
	if ((vmx_proc_ctrl2 & secondary_exec_addon) != secondary_exec_addon)
		return trace_error(-EIO);

	/* require PAT and EFER save/restore */
	vmx_entry_ctrl = read_msr(MSR_IA32_VMX_ENTRY_CTLS) >> 32;
	vmx_exit_ctrl = read_msr(MSR_IA32_VMX_EXIT_CTLS) >> 32;
	if (!(vmx_entry_ctrl & VM_ENTRY_LOAD_IA32_PAT) ||
	    !(vmx_entry_ctrl & VM_ENTRY_LOAD_IA32_EFER) ||
	    !(vmx_exit_ctrl & VM_EXIT_SAVE_IA32_PAT) ||
	    !(vmx_exit_ctrl & VM_EXIT_LOAD_IA32_PAT) ||
	    !(vmx_exit_ctrl & VM_EXIT_SAVE_IA32_EFER) ||
	    !(vmx_exit_ctrl & VM_EXIT_LOAD_IA32_EFER))
		return trace_error(-EIO);

	/* require activity state HLT */
	if (!(read_msr(MSR_IA32_VMX_MISC) & VMX_MISC_ACTIVITY_HLT))
		return trace_error(-EIO);

	/*
	 * Retrieve/validate restrictions on CR0
	 *
	 * In addition to what the VMX MSRs tell us, make sure that
	 * - NW and CD are kept off as they are not updated on VM exit and we
	 *   don't want them enabled for performance reasons while in root mode
	 * - PE and PG can be freely chosen (by the guest) because we demand
	 *   unrestricted guest mode support anyway
	 * - ET is always on (architectural requirement)
	 */
	maybe1 = read_msr(MSR_IA32_VMX_CR0_FIXED1) &
		~(X86_CR0_NW | X86_CR0_CD);
	required1 = (read_msr(MSR_IA32_VMX_CR0_FIXED0) &
		~(X86_CR0_PE | X86_CR0_PG)) | X86_CR0_ET;
	if (!vmx_define_cr_restrictions(CR0_IDX, maybe1, required1))
		return trace_error(-EIO);

	/* Retrieve/validate restrictions on CR4 */
	maybe1 = read_msr(MSR_IA32_VMX_CR4_FIXED1);
	required1 = read_msr(MSR_IA32_VMX_CR4_FIXED0);
	if (!vmx_define_cr_restrictions(CR4_IDX, maybe1, required1))
		return trace_error(-EIO);

	return 0;
}

static void ept_set_next_pt(pt_entry_t pte, unsigned long next_pt)
{
	*pte = (next_pt & BIT_MASK(51, 12)) | EPT_FLAG_READ | EPT_FLAG_WRITE |
		EPT_FLAG_EXECUTE;
}

int vcpu_vendor_early_init(void)
{
	unsigned int n;
	int err;

	err = vmx_check_features();
	if (err)
		return err;

	/* derive ept_paging from very similar x86_64_paging */
	memcpy(ept_paging, x86_64_paging, sizeof(ept_paging));
	for (n = 0; n < EPT_PAGE_DIR_LEVELS; n++)
		ept_paging[n].set_next_pt = ept_set_next_pt;
	if (!(read_msr(MSR_IA32_VMX_EPT_VPID_CAP) & EPT_1G_PAGES))
		ept_paging[1].page_size = 0;
	if (!(read_msr(MSR_IA32_VMX_EPT_VPID_CAP) & EPT_2M_PAGES))
		ept_paging[2].page_size = 0;

	parking_pt.root_paging = ept_paging;

	if (using_x2apic) {
		/* allow direct x2APIC access except for ICR writes */
		memset(&msr_bitmap[VMX_MSR_BMP_0000_READ][MSR_X2APIC_BASE/8],
		       0, (MSR_X2APIC_END - MSR_X2APIC_BASE + 1)/8);
		memset(&msr_bitmap[VMX_MSR_BMP_0000_WRITE][MSR_X2APIC_BASE/8],
		       0, (MSR_X2APIC_END - MSR_X2APIC_BASE + 1)/8);
		msr_bitmap[VMX_MSR_BMP_0000_WRITE][MSR_X2APIC_ICR/8] = 0x01;
	}

	return vcpu_cell_init(&root_cell);
}

unsigned long arch_paging_gphys2phys(unsigned long gphys, unsigned long flags)
{
	return paging_virt2phys(&this_cell()->arch.vmx.ept_structs, gphys,
				flags);
}

int vcpu_vendor_cell_init(struct cell *cell)
{
	int err;

	/* allocate io_bitmap */
	cell->arch.vmx.io_bitmap = page_alloc(&mem_pool, PIO_BITMAP_PAGES);
	if (!cell->arch.vmx.io_bitmap)
		return -ENOMEM;

	/* build root EPT of cell */
	cell->arch.vmx.ept_structs.root_paging = ept_paging;
	cell->arch.vmx.ept_structs.root_table =
		(page_table_t)cell->arch.root_table_page;

	/* Map the special APIC access page into the guest's physical address
	 * space at the default address (XAPIC_BASE) */
	err = paging_create(&cell->arch.vmx.ept_structs,
			    paging_hvirt2phys(apic_access_page),
			    PAGE_SIZE, XAPIC_BASE,
			    EPT_FLAG_READ | EPT_FLAG_WRITE | EPT_FLAG_WB_TYPE,
			    PAGING_NON_COHERENT);
	if (err)
		goto err_free_io_bitmap;

	return 0;

err_free_io_bitmap:
	page_free(&mem_pool, cell->arch.vmx.io_bitmap, 2);

	return err;
}

int vcpu_map_memory_region(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	u64 phys_start = mem->phys_start;
	u32 flags = EPT_FLAG_WB_TYPE;

	if (mem->flags & JAILHOUSE_MEM_READ)
		flags |= EPT_FLAG_READ;
	if (mem->flags & JAILHOUSE_MEM_WRITE)
		flags |= EPT_FLAG_WRITE;
	if (mem->flags & JAILHOUSE_MEM_EXECUTE)
		flags |= EPT_FLAG_EXECUTE;
	if (mem->flags & JAILHOUSE_MEM_COMM_REGION)
		phys_start = paging_hvirt2phys(&cell->comm_page);

	return paging_create(&cell->arch.vmx.ept_structs, phys_start, mem->size,
			     mem->virt_start, flags, PAGING_NON_COHERENT);
}

int vcpu_unmap_memory_region(struct cell *cell,
			     const struct jailhouse_memory *mem)
{
	return paging_destroy(&cell->arch.vmx.ept_structs, mem->virt_start,
			      mem->size, PAGING_NON_COHERENT);
}

void vcpu_vendor_cell_exit(struct cell *cell)
{
	paging_destroy(&cell->arch.vmx.ept_structs, XAPIC_BASE, PAGE_SIZE,
		       PAGING_NON_COHERENT);
	page_free(&mem_pool, cell->arch.vmx.io_bitmap, 2);
}

void vcpu_tlb_flush(void)
{
	unsigned long ept_cap = read_msr(MSR_IA32_VMX_EPT_VPID_CAP);
	struct {
		u64 eptp;
		u64 reserved;
	} descriptor;
	u64 type;
	u8 ok;

	descriptor.reserved = 0;
	if (ept_cap & EPT_INVEPT_SINGLE) {
		type = VMX_INVEPT_SINGLE;
		descriptor.eptp = vmcs_read64(EPT_POINTER);
	} else {
		type = VMX_INVEPT_GLOBAL;
		descriptor.eptp = 0;
	}
	asm volatile(
		"invept (%1),%2\n\t"
		"seta %0\n\t"
		: "=qm" (ok)
		: "r" (&descriptor), "r" (type)
		: "memory", "cc");

	if (!ok) {
		panic_printk("FATAL: invept failed, error %d\n",
			     vmcs_read32(VM_INSTRUCTION_ERROR));
		panic_stop();
	}
}

static bool vmx_set_guest_cr(unsigned int cr_idx, unsigned long val)
{
	bool ok = true;

	if (cr_idx)
		val |= X86_CR4_VMXE; /* keeps the hypervisor visible */

	ok &= vmcs_write64(cr_idx ? GUEST_CR4 : GUEST_CR0,
			   (val & cr_maybe1[cr_idx]) | cr_required1[cr_idx]);
	ok &= vmcs_write64(cr_idx ? CR4_READ_SHADOW : CR0_READ_SHADOW, val);
	ok &= vmcs_write64(cr_idx ? CR4_GUEST_HOST_MASK : CR0_GUEST_HOST_MASK,
			   cr_required1[cr_idx] | ~cr_maybe1[cr_idx]);

	return ok;
}

unsigned long vcpu_vendor_get_guest_cr4(void)
{
	unsigned long host_mask = cr_required1[CR4_IDX] | ~cr_maybe1[CR4_IDX];

	return (vmcs_read64(CR4_READ_SHADOW) & host_mask) |
		(vmcs_read64(GUEST_CR4) & ~host_mask);
}

static bool vmx_set_cell_config(void)
{
	struct cell *cell = this_cell();
	u8 *io_bitmap;
	bool ok = true;

	io_bitmap = cell->arch.vmx.io_bitmap;
	ok &= vmcs_write64(IO_BITMAP_A, paging_hvirt2phys(io_bitmap));
	ok &= vmcs_write64(IO_BITMAP_B,
			   paging_hvirt2phys(io_bitmap + PAGE_SIZE));

	ok &= vmcs_write64(EPT_POINTER,
		paging_hvirt2phys(cell->arch.vmx.ept_structs.root_table) |
		EPT_TYPE_WRITEBACK | EPT_PAGE_WALK_LEN);

	return ok;
}

static bool vmx_set_guest_segment(const struct segment *seg,
				  unsigned long selector_field)
{
	bool ok = true;

	ok &= vmcs_write16(selector_field, seg->selector);
	ok &= vmcs_write64(selector_field + GUEST_SEG_BASE, seg->base);
	ok &= vmcs_write32(selector_field + GUEST_SEG_LIMIT, seg->limit);
	ok &= vmcs_write32(selector_field + GUEST_SEG_AR_BYTES,
			   seg->access_rights);
	return ok;
}

static bool vmcs_setup(void)
{
	struct per_cpu *cpu_data = this_cpu_data();
	struct desc_table_reg dtr;
	unsigned long val;
	bool ok = true;

	ok &= vmcs_write64(HOST_CR0, read_cr0());
	ok &= vmcs_write64(HOST_CR3, read_cr3());
	ok &= vmcs_write64(HOST_CR4, read_cr4());

	ok &= vmcs_write16(HOST_CS_SELECTOR, GDT_DESC_CODE * 8);
	ok &= vmcs_write16(HOST_DS_SELECTOR, 0);
	ok &= vmcs_write16(HOST_ES_SELECTOR, 0);
	ok &= vmcs_write16(HOST_SS_SELECTOR, 0);
	ok &= vmcs_write16(HOST_FS_SELECTOR, 0);
	ok &= vmcs_write16(HOST_GS_SELECTOR, 0);
	ok &= vmcs_write16(HOST_TR_SELECTOR, GDT_DESC_TSS * 8);

	ok &= vmcs_write64(HOST_FS_BASE, 0);
	ok &= vmcs_write64(HOST_GS_BASE, read_msr(MSR_GS_BASE));
	ok &= vmcs_write64(HOST_TR_BASE, 0);

	read_gdtr(&dtr);
	ok &= vmcs_write64(HOST_GDTR_BASE, dtr.base);
	read_idtr(&dtr);
	ok &= vmcs_write64(HOST_IDTR_BASE, dtr.base);

	ok &= vmcs_write64(HOST_IA32_PAT, read_msr(MSR_IA32_PAT));
	ok &= vmcs_write64(HOST_IA32_EFER, EFER_LMA | EFER_LME);

	ok &= vmcs_write32(HOST_IA32_SYSENTER_CS, 0);
	ok &= vmcs_write64(HOST_IA32_SYSENTER_EIP, 0);
	ok &= vmcs_write64(HOST_IA32_SYSENTER_ESP, 0);

	ok &= vmcs_write64(HOST_RSP, (unsigned long)cpu_data->stack +
			   sizeof(cpu_data->stack));

	/* Set function executed when trapping to the hypervisor */
	ok &= vmcs_write64(HOST_RIP, (unsigned long)vmx_vmexit);

	ok &= vmx_set_guest_cr(CR0_IDX, cpu_data->linux_cr0);
	ok &= vmx_set_guest_cr(CR4_IDX, cpu_data->linux_cr4);

	ok &= vmcs_write64(GUEST_CR3, cpu_data->linux_cr3);

	ok &= vmx_set_guest_segment(&cpu_data->linux_cs, GUEST_CS_SELECTOR);
	ok &= vmx_set_guest_segment(&cpu_data->linux_ds, GUEST_DS_SELECTOR);
	ok &= vmx_set_guest_segment(&cpu_data->linux_es, GUEST_ES_SELECTOR);
	ok &= vmx_set_guest_segment(&cpu_data->linux_fs, GUEST_FS_SELECTOR);
	ok &= vmx_set_guest_segment(&cpu_data->linux_gs, GUEST_GS_SELECTOR);
	ok &= vmx_set_guest_segment(&invalid_seg, GUEST_SS_SELECTOR);
	ok &= vmx_set_guest_segment(&cpu_data->linux_tss, GUEST_TR_SELECTOR);
	ok &= vmx_set_guest_segment(&invalid_seg, GUEST_LDTR_SELECTOR);

	ok &= vmcs_write64(GUEST_GDTR_BASE, cpu_data->linux_gdtr.base);
	ok &= vmcs_write32(GUEST_GDTR_LIMIT, cpu_data->linux_gdtr.limit);
	ok &= vmcs_write64(GUEST_IDTR_BASE, cpu_data->linux_idtr.base);
	ok &= vmcs_write32(GUEST_IDTR_LIMIT, cpu_data->linux_idtr.limit);

	ok &= vmcs_write64(GUEST_RFLAGS, 0x02);
	ok &= vmcs_write64(GUEST_RSP, cpu_data->linux_sp +
			   (NUM_ENTRY_REGS + 1) * sizeof(unsigned long));
	ok &= vmcs_write64(GUEST_RIP, cpu_data->linux_ip);

	ok &= vmcs_write32(GUEST_SYSENTER_CS,
			   read_msr(MSR_IA32_SYSENTER_CS));
	ok &= vmcs_write64(GUEST_SYSENTER_EIP,
			   read_msr(MSR_IA32_SYSENTER_EIP));
	ok &= vmcs_write64(GUEST_SYSENTER_ESP,
			   read_msr(MSR_IA32_SYSENTER_ESP));

	ok &= vmcs_write64(GUEST_DR7, 0x00000400);
	ok &= vmcs_write64(GUEST_IA32_DEBUGCTL, 0);

	ok &= vmcs_write32(GUEST_ACTIVITY_STATE, GUEST_ACTIVITY_ACTIVE);
	ok &= vmcs_write32(GUEST_INTERRUPTIBILITY_INFO, 0);
	ok &= vmcs_write64(GUEST_PENDING_DBG_EXCEPTIONS, 0);

	ok &= vmcs_write64(GUEST_IA32_PAT, cpu_data->pat);
	ok &= vmcs_write64(GUEST_IA32_EFER, cpu_data->linux_efer);

	ok &= vmcs_write64(VMCS_LINK_POINTER, -1UL);
	ok &= vmcs_write32(VM_ENTRY_INTR_INFO_FIELD, 0);

	val = read_msr(MSR_IA32_VMX_PINBASED_CTLS);
	val |= PIN_BASED_NMI_EXITING;
	ok &= vmcs_write32(PIN_BASED_VM_EXEC_CONTROL, val);

	ok &= vmcs_write32(VMX_PREEMPTION_TIMER_VALUE, 0);

	val = read_msr(MSR_IA32_VMX_PROCBASED_CTLS);
	val |= CPU_BASED_USE_IO_BITMAPS | CPU_BASED_USE_MSR_BITMAPS |
		CPU_BASED_ACTIVATE_SECONDARY_CONTROLS;
	val &= ~(CPU_BASED_CR3_LOAD_EXITING | CPU_BASED_CR3_STORE_EXITING);
	ok &= vmcs_write32(CPU_BASED_VM_EXEC_CONTROL, val);

	ok &= vmcs_write64(MSR_BITMAP, paging_hvirt2phys(msr_bitmap));

	val = read_msr(MSR_IA32_VMX_PROCBASED_CTLS2);
	val |= SECONDARY_EXEC_VIRTUALIZE_APIC_ACCESSES |
		SECONDARY_EXEC_ENABLE_EPT | SECONDARY_EXEC_UNRESTRICTED_GUEST |
		secondary_exec_addon;
	ok &= vmcs_write32(SECONDARY_VM_EXEC_CONTROL, val);

	ok &= vmcs_write64(APIC_ACCESS_ADDR,
			   paging_hvirt2phys(apic_access_page));

	ok &= vmx_set_cell_config();

	/* see vmx_handle_exception_nmi for the interception reason */
	ok &= vmcs_write32(EXCEPTION_BITMAP,
			   (1 << DB_VECTOR) | (1 << AC_VECTOR));

	val = read_msr(MSR_IA32_VMX_EXIT_CTLS);
	val |= VM_EXIT_HOST_ADDR_SPACE_SIZE |
		VM_EXIT_SAVE_IA32_PAT | VM_EXIT_LOAD_IA32_PAT |
		VM_EXIT_SAVE_IA32_EFER | VM_EXIT_LOAD_IA32_EFER;
	ok &= vmcs_write32(VM_EXIT_CONTROLS, val);

	ok &= vmcs_write32(VM_EXIT_MSR_STORE_COUNT, 0);
	ok &= vmcs_write32(VM_EXIT_MSR_LOAD_COUNT, 0);
	ok &= vmcs_write32(VM_ENTRY_MSR_LOAD_COUNT, 0);

	val = read_msr(MSR_IA32_VMX_ENTRY_CTLS);
	val |= VM_ENTRY_IA32E_MODE | VM_ENTRY_LOAD_IA32_PAT |
		VM_ENTRY_LOAD_IA32_EFER;
	ok &= vmcs_write32(VM_ENTRY_CONTROLS, val);

	ok &= vmcs_write64(CR4_GUEST_HOST_MASK, 0);

	ok &= vmcs_write32(CR3_TARGET_COUNT, 0);

	return ok;
}

int vcpu_init(struct per_cpu *cpu_data)
{
	unsigned long feature_ctrl, mask;
	u32 revision_id;
	int err;

	/* make sure all perf counters are off */
	if ((cpuid_eax(0x0a, 0) & 0xff) > 0)
		write_msr(MSR_IA32_PERF_GLOBAL_CTRL, 0);

	if (cpu_data->linux_cr4 & X86_CR4_VMXE)
		return trace_error(-EBUSY);

	err = vmx_check_features();
	if (err)
		return err;

	revision_id = (u32)read_msr(MSR_IA32_VMX_BASIC);
	cpu_data->vmxon_region.revision_id = revision_id;
	cpu_data->vmxon_region.shadow_indicator = 0;
	cpu_data->vmcs.revision_id = revision_id;
	cpu_data->vmcs.shadow_indicator = 0;

	/* Note: We assume that TXT is off */
	feature_ctrl = read_msr(MSR_IA32_FEATURE_CONTROL);
	mask = FEATURE_CONTROL_LOCKED |
		FEATURE_CONTROL_VMXON_ENABLED_OUTSIDE_SMX;

	if ((feature_ctrl & mask) != mask) {
		if (feature_ctrl & FEATURE_CONTROL_LOCKED)
			return trace_error(-ENODEV);

		feature_ctrl |= mask;
		write_msr(MSR_IA32_FEATURE_CONTROL, feature_ctrl);
	}

	/*
	 * SDM Volume 3, 2.5: "When loading a control register, reserved bits
	 * should always be set to the values previously read."
	 * But we want to avoid surprises with new features unknown to us but
	 * set by Linux. So check if any assumed revered bit was set or should
	 * be set for VMX operation and bail out if so.
	 */
	if ((cpu_data->linux_cr0 | cr_required1[CR0_IDX]) & X86_CR0_RESERVED ||
	    (cpu_data->linux_cr4 | cr_required1[CR4_IDX]) & X86_CR4_RESERVED)
		return trace_error(-EIO);
	/*
	 * Bring CR0 and CR4 into well-defined states. If they do not match
	 * with VMX requirements, vmxon will fail.
	 * X86_CR4_OSXSAVE is enabled if available so that xsetbv can be
	 * executed on behalf of a cell.
	 */
	write_cr0(X86_CR0_HOST_STATE);
	write_cr4(X86_CR4_HOST_STATE | X86_CR4_VMXE |
		  ((cpuid_ecx(1, 0) & X86_FEATURE_XSAVE) ?
		   X86_CR4_OSXSAVE : 0));

	if (!vmxon())  {
		write_cr4(cpu_data->linux_cr4);
		return trace_error(-EIO);
	}

	cpu_data->vmx_state = VMXON;

	if (!vmcs_clear() || !vmcs_load() || !vmcs_setup())
		return trace_error(-EIO);

	cpu_data->vmx_state = VMCS_READY;

	return 0;
}

void vcpu_exit(struct per_cpu *cpu_data)
{
	if (cpu_data->vmx_state == VMXOFF)
		return;

	cpu_data->vmx_state = VMXOFF;
	/* Write vmx_state to ensure that vcpu_nmi_handler stops accessing
	 * the VMCS (a compiler barrier would be sufficient, in fact). */
	memory_barrier();

	vmcs_clear();
	asm volatile("vmxoff" : : : "cc");
	cpu_data->linux_cr4 &= ~X86_CR4_VMXE;
}

void __attribute__((noreturn)) vcpu_activate_vmm(void)
{
	/* We enter Linux at the point arch_entry would return to as well.
	 * rax is cleared to signal success to the caller. */
	asm volatile(
		"mov (%%rdi),%%r15\n\t"
		"mov 0x8(%%rdi),%%r14\n\t"
		"mov 0x10(%%rdi),%%r13\n\t"
		"mov 0x18(%%rdi),%%r12\n\t"
		"mov 0x20(%%rdi),%%rbx\n\t"
		"mov 0x28(%%rdi),%%rbp\n\t"
		"vmlaunch\n\t"
		"pop %%rbp"
		: /* no output */
		: "a" (0), "D" (this_cpu_data()->linux_reg)
		: "memory", "r15", "r14", "r13", "r12", "rbx", "rbp", "cc");

	panic_printk("FATAL: vmlaunch failed, error %d\n",
		     vmcs_read32(VM_INSTRUCTION_ERROR));
	panic_stop();
}

void __attribute__((noreturn)) vcpu_deactivate_vmm(void)
{
	unsigned long *stack = (unsigned long *)vmcs_read64(GUEST_RSP);
	unsigned long linux_ip = vmcs_read64(GUEST_RIP);
	struct per_cpu *cpu_data = this_cpu_data();

	cpu_data->linux_cr0 = vmcs_read64(GUEST_CR0);
	cpu_data->linux_cr3 = vmcs_read64(GUEST_CR3);
	cpu_data->linux_cr4 = vmcs_read64(GUEST_CR4);

	cpu_data->linux_gdtr.base = vmcs_read64(GUEST_GDTR_BASE);
	cpu_data->linux_gdtr.limit = vmcs_read64(GUEST_GDTR_LIMIT);
	cpu_data->linux_idtr.base = vmcs_read64(GUEST_IDTR_BASE);
	cpu_data->linux_idtr.limit = vmcs_read64(GUEST_IDTR_LIMIT);

	cpu_data->linux_cs.selector = vmcs_read32(GUEST_CS_SELECTOR);

	cpu_data->linux_tss.selector = vmcs_read32(GUEST_TR_SELECTOR);

	cpu_data->linux_efer = vmcs_read64(GUEST_IA32_EFER);
	cpu_data->linux_fs.base = vmcs_read64(GUEST_FS_BASE);
	cpu_data->linux_gs.base = vmcs_read64(GUEST_GS_BASE);

	write_msr(MSR_IA32_SYSENTER_CS, vmcs_read32(GUEST_SYSENTER_CS));
	write_msr(MSR_IA32_SYSENTER_EIP, vmcs_read64(GUEST_SYSENTER_EIP));
	write_msr(MSR_IA32_SYSENTER_ESP, vmcs_read64(GUEST_SYSENTER_ESP));

	cpu_data->linux_ds.selector = vmcs_read16(GUEST_DS_SELECTOR);
	cpu_data->linux_es.selector = vmcs_read16(GUEST_ES_SELECTOR);
	cpu_data->linux_fs.selector = vmcs_read16(GUEST_FS_SELECTOR);
	cpu_data->linux_gs.selector = vmcs_read16(GUEST_GS_SELECTOR);

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
	unsigned long reset_addr, val;
	bool ok = true;

	ok &= vmx_set_guest_cr(CR0_IDX, X86_CR0_NW | X86_CR0_CD | X86_CR0_ET);
	ok &= vmx_set_guest_cr(CR4_IDX, 0);

	ok &= vmcs_write64(GUEST_CR3, 0);

	ok &= vmcs_write64(GUEST_RFLAGS, 0x02);
	ok &= vmcs_write64(GUEST_RSP, 0);

	if (sipi_vector == APIC_BSP_PSEUDO_SIPI) {
		/* only cleared on hard reset */
		ok &= vmcs_write64(GUEST_IA32_DEBUGCTL, 0);

		reset_addr = this_cell()->config->cpu_reset_address;

		ok &= vmcs_write64(GUEST_RIP, reset_addr & 0xffff);

		ok &= vmcs_write16(GUEST_CS_SELECTOR,
				   (reset_addr >> 4) & 0xf000);
		ok &= vmcs_write64(GUEST_CS_BASE, reset_addr & ~0xffffL);
	} else {
		ok &= vmcs_write64(GUEST_RIP, 0);

		ok &= vmcs_write16(GUEST_CS_SELECTOR, sipi_vector << 8);
		ok &= vmcs_write64(GUEST_CS_BASE, sipi_vector << 12);
	}

	ok &= vmcs_write32(GUEST_CS_LIMIT, 0xffff);
	ok &= vmcs_write32(GUEST_CS_AR_BYTES, 0x0009b);

	ok &= vmcs_write16(GUEST_DS_SELECTOR, 0);
	ok &= vmcs_write64(GUEST_DS_BASE, 0);
	ok &= vmcs_write32(GUEST_DS_LIMIT, 0xffff);
	ok &= vmcs_write32(GUEST_DS_AR_BYTES, 0x00093);

	ok &= vmcs_write16(GUEST_ES_SELECTOR, 0);
	ok &= vmcs_write64(GUEST_ES_BASE, 0);
	ok &= vmcs_write32(GUEST_ES_LIMIT, 0xffff);
	ok &= vmcs_write32(GUEST_ES_AR_BYTES, 0x00093);

	ok &= vmcs_write16(GUEST_FS_SELECTOR, 0);
	ok &= vmcs_write64(GUEST_FS_BASE, 0);
	ok &= vmcs_write32(GUEST_FS_LIMIT, 0xffff);
	ok &= vmcs_write32(GUEST_FS_AR_BYTES, 0x00093);

	ok &= vmcs_write16(GUEST_GS_SELECTOR, 0);
	ok &= vmcs_write64(GUEST_GS_BASE, 0);
	ok &= vmcs_write32(GUEST_GS_LIMIT, 0xffff);
	ok &= vmcs_write32(GUEST_GS_AR_BYTES, 0x00093);

	ok &= vmcs_write16(GUEST_SS_SELECTOR, 0);
	ok &= vmcs_write64(GUEST_SS_BASE, 0);
	ok &= vmcs_write32(GUEST_SS_LIMIT, 0xffff);
	ok &= vmcs_write32(GUEST_SS_AR_BYTES, 0x00093);

	ok &= vmcs_write16(GUEST_TR_SELECTOR, 0);
	ok &= vmcs_write64(GUEST_TR_BASE, 0);
	ok &= vmcs_write32(GUEST_TR_LIMIT, 0xffff);
	ok &= vmcs_write32(GUEST_TR_AR_BYTES, 0x0008b);

	ok &= vmcs_write16(GUEST_LDTR_SELECTOR, 0);
	ok &= vmcs_write64(GUEST_LDTR_BASE, 0);
	ok &= vmcs_write32(GUEST_LDTR_LIMIT, 0xffff);
	ok &= vmcs_write32(GUEST_LDTR_AR_BYTES, 0x00082);

	ok &= vmcs_write64(GUEST_GDTR_BASE, 0);
	ok &= vmcs_write32(GUEST_GDTR_LIMIT, 0xffff);
	ok &= vmcs_write64(GUEST_IDTR_BASE, 0);
	ok &= vmcs_write32(GUEST_IDTR_LIMIT, 0xffff);

	ok &= vmcs_write64(GUEST_IA32_EFER, 0);

	ok &= vmcs_write32(GUEST_SYSENTER_CS, 0);
	ok &= vmcs_write64(GUEST_SYSENTER_EIP, 0);
	ok &= vmcs_write64(GUEST_SYSENTER_ESP, 0);

	ok &= vmcs_write64(GUEST_DR7, 0x00000400);

	ok &= vmcs_write32(GUEST_ACTIVITY_STATE, GUEST_ACTIVITY_ACTIVE);
	ok &= vmcs_write32(GUEST_INTERRUPTIBILITY_INFO, 0);
	ok &= vmcs_write64(GUEST_PENDING_DBG_EXCEPTIONS, 0);
	ok &= vmcs_write32(VM_ENTRY_INTR_INFO_FIELD, 0);

	val = vmcs_read32(VM_ENTRY_CONTROLS);
	val &= ~VM_ENTRY_IA32E_MODE;
	ok &= vmcs_write32(VM_ENTRY_CONTROLS, val);

	ok &= vmx_set_cell_config();

	if (!ok) {
		panic_printk("FATAL: CPU reset failed\n");
		panic_stop();
	}
}

static void vmx_preemption_timer_set_enable(bool enable)
{
	u32 pin_based_ctrl = vmcs_read32(PIN_BASED_VM_EXEC_CONTROL);

	if (enable)
		pin_based_ctrl |= PIN_BASED_VMX_PREEMPTION_TIMER;
	else
		pin_based_ctrl &= ~PIN_BASED_VMX_PREEMPTION_TIMER;
	vmcs_write32(PIN_BASED_VM_EXEC_CONTROL, pin_based_ctrl);
}

void vcpu_nmi_handler(void)
{
	if (this_cpu_data()->vmx_state == VMCS_READY)
		vmx_preemption_timer_set_enable(true);
}

void vcpu_park(void)
{
#ifdef CONFIG_CRASH_CELL_ON_PANIC
	if (this_cpu_data()->failed) {
		vmcs_write64(GUEST_RIP, 0);
		return;
	}
#endif
	vcpu_vendor_reset(0);
	vmcs_write64(GUEST_IA32_DEBUGCTL, 0);
	vmcs_write64(EPT_POINTER, paging_hvirt2phys(parking_pt.root_table) |
				  EPT_TYPE_WRITEBACK | EPT_PAGE_WALK_LEN);
}

void vcpu_skip_emulated_instruction(unsigned int inst_len)
{
	vmcs_write64(GUEST_RIP, vmcs_read64(GUEST_RIP) + inst_len);
}

static void vmx_check_events(void)
{
	vmx_preemption_timer_set_enable(false);
	x86_check_events();
}

static void vmx_handle_exception_nmi(void)
{
	u32 intr_info = vmcs_read32(VM_EXIT_INTR_INFO);

	if ((intr_info & INTR_INFO_INTR_TYPE_MASK) == INTR_TYPE_NMI_INTR) {
		this_cpu_data()->stats[JAILHOUSE_CPU_STAT_VMEXITS_MANAGEMENT]++;
		asm volatile("int %0" : : "i" (NMI_VECTOR));
	} else {
		this_cpu_data()->stats[JAILHOUSE_CPU_STAT_VMEXITS_EXCEPTION]++;
		/*
		 * Reinject the event straight away. We only intercept #DB and
		 * #AC to prevent that malicious guests can trigger infinite
		 * loops in microcode (see e.g. CVE-2015-5307 and
		 * CVE-2015-8104).
		 */
		vmcs_write32(VM_ENTRY_INTR_INFO_FIELD,
			     intr_info & INTR_TO_VECTORING_INFO_MASK);
		vmcs_write32(VM_ENTRY_EXCEPTION_ERROR_CODE,
			     vmcs_read32(VM_EXIT_INTR_ERROR_CODE));
	}

	/*
	 * Check for events even in the exception case in order to maintain
	 * control over the guest if it triggered #DB or #AC loops.
	 */
	vmx_check_events();
}

static void update_efer(void)
{
	unsigned long efer = vmcs_read64(GUEST_IA32_EFER);

	if ((efer & (EFER_LME | EFER_LMA)) != EFER_LME)
		return;

	efer |= EFER_LMA;
	vmcs_write64(GUEST_IA32_EFER, efer);
	vmcs_write32(VM_ENTRY_CONTROLS,
		     vmcs_read32(VM_ENTRY_CONTROLS) | VM_ENTRY_IA32E_MODE);
}

static bool vmx_handle_cr(void)
{
	u64 exit_qualification = vmcs_read64(EXIT_QUALIFICATION);
	unsigned long cr, reg, val;

	cr = exit_qualification & 0xf;
	reg = (exit_qualification >> 8) & 0xf;

	switch ((exit_qualification >> 4) & 3) {
	case 0: /* move to cr */
		if (reg == 4)
			val = vmcs_read64(GUEST_RSP);
		else
			val = this_cpu_data()->guest_regs.by_index[15 - reg];

		if (cr == 0 || cr == 4) {
			vcpu_skip_emulated_instruction(X86_INST_LEN_MOV_TO_CR);
			/* TODO: check for #GP reasons */
			vmx_set_guest_cr(cr ? CR4_IDX : CR0_IDX, val);
			if (cr == 0 && val & X86_CR0_PG)
				update_efer();
			return true;
		}
		break;
	default:
		break;
	}
	panic_printk("FATAL: Unhandled CR access, qualification %llx\n",
		     exit_qualification);
	return false;
}

void vcpu_get_guest_paging_structs(struct guest_paging_structures *pg_structs)
{
	struct per_cpu *cpu_data = this_cpu_data();
	unsigned int n;

	if (vmcs_read32(VM_ENTRY_CONTROLS) & VM_ENTRY_IA32E_MODE) {
		pg_structs->root_paging = x86_64_paging;
		pg_structs->root_table_gphys =
			vmcs_read64(GUEST_CR3) & BIT_MASK(51, 12);
	} else if (!(vmcs_read64(GUEST_CR0) & X86_CR0_PG)) {
		pg_structs->root_paging = NULL;
	} else if (vmcs_read64(GUEST_CR4) & X86_CR4_PAE) {
		pg_structs->root_paging = pae_paging;
		/*
		 * Although we read the PDPTEs from the guest registers, we
		 * need to provide the root table here to please the generic
		 * page table walker. It will map the PDPT then, but we won't
		 * use it.
		 */
		pg_structs->root_table_gphys =
			vmcs_read64(GUEST_CR3) & BIT_MASK(31, 5);
		/*
		 * The CPU caches the PDPTEs in registers. We need to use them
		 * instead of reading the entries from guest memory.
		 */
		for (n = 0; n < 4; n++)
			cpu_data->pdpte[n] = vmcs_read64(GUEST_PDPTR0 + n * 2);
	} else {
		pg_structs->root_paging = i386_paging;
		pg_structs->root_table_gphys =
			vmcs_read64(GUEST_CR3) & BIT_MASK(31, 12);
	}
}

pt_entry_t vcpu_pae_get_pdpte(page_table_t page_table, unsigned long virt)
{
	return &this_cpu_data()->pdpte[(virt >> 30) & 0x3];
}

void vcpu_vendor_set_guest_pat(unsigned long val)
{
	vmcs_write64(GUEST_IA32_PAT, val);
}

static bool vmx_handle_apic_access(void)
{
	struct guest_paging_structures pg_structs;
	unsigned int inst_len, offset;
	u64 qualification;
	bool is_write;

	qualification = vmcs_read64(EXIT_QUALIFICATION);

	switch (qualification & APIC_ACCESS_TYPE_MASK) {
	case APIC_ACCESS_TYPE_LINEAR_READ:
	case APIC_ACCESS_TYPE_LINEAR_WRITE:
		is_write = !!(qualification & APIC_ACCESS_TYPE_LINEAR_WRITE);
		offset = qualification & APIC_ACCESS_OFFSET_MASK;
		if (offset & 0x00f)
			break;

		vcpu_get_guest_paging_structs(&pg_structs);

		inst_len = apic_mmio_access(&pg_structs, offset >> 4, is_write);
		if (!inst_len)
			break;

		vcpu_skip_emulated_instruction(inst_len);
		return true;
	}
	panic_printk("FATAL: Unhandled APIC access, "
		     "qualification %llx\n", qualification);
	return false;
}

static bool vmx_handle_xsetbv(void)
{
	union registers *guest_regs = &this_cpu_data()->guest_regs;

	if (vcpu_vendor_get_guest_cr4() & X86_CR4_OSXSAVE &&
	    guest_regs->rax & X86_XCR0_FP &&
	    (guest_regs->rax & ~cpuid_eax(0x0d, 0)) == 0 &&
	     guest_regs->rcx == 0 && guest_regs->rdx == 0) {
		vcpu_skip_emulated_instruction(X86_INST_LEN_XSETBV);
		asm volatile(
			"xsetbv"
			: /* no output */
			: "a" (guest_regs->rax), "c" (0), "d" (0));
		return true;
	}
	panic_printk("FATAL: Invalid xsetbv parameters: "
		     "xcr[%ld] = %08lx:%08lx\n",
		     guest_regs->rcx, guest_regs->rdx, guest_regs->rax);
	return false;
}

static void dump_vm_exit_details(u32 reason)
{
	panic_printk("qualification %lx\n", vmcs_read64(EXIT_QUALIFICATION));
	panic_printk("vectoring info: %x interrupt info: %x\n",
		     vmcs_read32(IDT_VECTORING_INFO_FIELD),
		     vmcs_read32(VM_EXIT_INTR_INFO));
	if (reason == EXIT_REASON_EPT_VIOLATION ||
	    reason == EXIT_REASON_EPT_MISCONFIG)
		panic_printk("guest phys: 0x%016lx guest linear: 0x%016lx\n",
			     vmcs_read64(GUEST_PHYSICAL_ADDRESS),
			     vmcs_read64(GUEST_LINEAR_ADDRESS));
}

static void dump_guest_regs(union registers *guest_regs)
{
	panic_printk("RIP: 0x%016lx RSP: 0x%016lx FLAGS: %lx\n",
		     vmcs_read64(GUEST_RIP), vmcs_read64(GUEST_RSP),
		     vmcs_read64(GUEST_RFLAGS));
	panic_printk("RAX: 0x%016lx RBX: 0x%016lx RCX: 0x%016lx\n",
		     guest_regs->rax, guest_regs->rbx, guest_regs->rcx);
	panic_printk("RDX: 0x%016lx RSI: 0x%016lx RDI: 0x%016lx\n",
		     guest_regs->rdx, guest_regs->rsi, guest_regs->rdi);
	panic_printk("CS: %lx BASE: 0x%016lx AR-BYTES: %x EFER.LMA %d\n",
		     vmcs_read64(GUEST_CS_SELECTOR),
		     vmcs_read64(GUEST_CS_BASE),
		     vmcs_read32(GUEST_CS_AR_BYTES),
		     !!(vmcs_read32(VM_ENTRY_CONTROLS) & VM_ENTRY_IA32E_MODE));
	panic_printk("CR0: 0x%016lx CR3: 0x%016lx CR4: 0x%016lx\n",
		     vmcs_read64(GUEST_CR0), vmcs_read64(GUEST_CR3),
		     vmcs_read64(GUEST_CR4));
	panic_printk("EFER: 0x%016lx\n", vmcs_read64(GUEST_IA32_EFER));
}

void vcpu_vendor_get_io_intercept(struct vcpu_io_intercept *io)
{
	u64 exitq = vmcs_read64(EXIT_QUALIFICATION);

	/* parse exit qualification for I/O instructions (see SDM, 27.2.1 ) */
	io->port = (exitq >> 16) & 0xFFFF;
	io->size = (exitq & 0x3) + 1;
	io->in = !!((exitq & 0x8) >> 3);
	io->inst_len = vmcs_read64(VM_EXIT_INSTRUCTION_LEN);
	io->rep_or_str = !!(exitq & 0x30);
}

void vcpu_vendor_get_mmio_intercept(struct vcpu_mmio_intercept *mmio)
{
	u64 exitq = vmcs_read64(EXIT_QUALIFICATION);

	mmio->phys_addr = vmcs_read64(GUEST_PHYSICAL_ADDRESS);
	/* We don't enable dirty/accessed bit updated in EPTP,
	 * so only read of write flags can be set, not both. */
	mmio->is_write = !!(exitq & 0x2);
}

void vcpu_handle_exit(struct per_cpu *cpu_data)
{
	u32 reason = vmcs_read32(VM_EXIT_REASON);

	cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_TOTAL]++;

	switch (reason) {
	case EXIT_REASON_EXCEPTION_NMI:
		vmx_handle_exception_nmi();
		return;
	case EXIT_REASON_PREEMPTION_TIMER:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_MANAGEMENT]++;
		vmx_check_events();
		return;
	case EXIT_REASON_CPUID:
		vcpu_handle_cpuid();
		return;
	case EXIT_REASON_VMCALL:
		vcpu_handle_hypercall();
		return;
	case EXIT_REASON_CR_ACCESS:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_CR]++;
		if (vmx_handle_cr())
			return;
		break;
	case EXIT_REASON_MSR_READ:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_MSR]++;
		if (vcpu_handle_msr_read())
			return;
		break;
	case EXIT_REASON_MSR_WRITE:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_MSR]++;
		if (cpu_data->guest_regs.rcx == MSR_IA32_PERF_GLOBAL_CTRL) {
			/* ignore writes */
			vcpu_skip_emulated_instruction(X86_INST_LEN_WRMSR);
			return;
		} else if (vcpu_handle_msr_write())
			return;
		break;
	case EXIT_REASON_APIC_ACCESS:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_XAPIC]++;
		if (vmx_handle_apic_access())
			return;
		break;
	case EXIT_REASON_XSETBV:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_XSETBV]++;
		if (vmx_handle_xsetbv())
			return;
		break;
	case EXIT_REASON_IO_INSTRUCTION:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_PIO]++;
		if (vcpu_handle_io_access())
			return;
		break;
	case EXIT_REASON_EPT_VIOLATION:
		cpu_data->stats[JAILHOUSE_CPU_STAT_VMEXITS_MMIO]++;
		if (vcpu_handle_mmio_access())
			return;
		break;
	default:
		panic_printk("FATAL: %s, reason %d\n",
			     (reason & EXIT_REASONS_FAILED_VMENTRY) ?
			     "VM-Entry failure" : "Unhandled VM-Exit",
			     (u16)reason);
		dump_vm_exit_details(reason);
		break;
	}
	dump_guest_regs(&cpu_data->guest_regs);
	panic_park();
}

void vmx_entry_failure(void)
{
	panic_printk("FATAL: vmresume failed, error %d\n",
		     vmcs_read32(VM_INSTRUCTION_ERROR));
	panic_stop();
}

void vcpu_vendor_get_cell_io_bitmap(struct cell *cell,
		                    struct vcpu_io_bitmap *iobm)
{
	iobm->data = cell->arch.vmx.io_bitmap;
	iobm->size = PIO_BITMAP_PAGES * PAGE_SIZE;
}

#define VCPU_VENDOR_GET_REGISTER(__reg__, __field__)	\
u64 vcpu_vendor_get_##__reg__(void)			\
{							\
	return vmcs_read64(__field__);			\
}

VCPU_VENDOR_GET_REGISTER(efer, GUEST_IA32_EFER);
VCPU_VENDOR_GET_REGISTER(rflags, GUEST_RFLAGS);
VCPU_VENDOR_GET_REGISTER(rip, GUEST_RIP);

u16 vcpu_vendor_get_cs_attr(void)
{
	return vmcs_read32(GUEST_CS_AR_BYTES);
}

void enable_irq(void)
{
	asm volatile("sti" : : : "memory");
}

void disable_irq(void)
{
	asm volatile("cli" : : : "memory");
}
