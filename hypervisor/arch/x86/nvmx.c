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

#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/percpu.h>
#include <jailhouse/processor.h>
#include <jailhouse/printk.h>
#include <jailhouse/utils.h>
#include <asm/vcpu.h>
#include <asm/vmx.h>

#define FIELD_FLAG_VALID	0x01
#define FIELD_FLAG_SHADOWED	0x02
#define FIELD_FLAG_READONLY	0x04
#define FIELD_FLAG_HWUPDATED	0x08

#define FIELD_INVALID		0x00
#define FIELD_DIRECT		FIELD_FLAG_VALID
#define FIELD_DIRECT_HWUPD	(FIELD_FLAG_VALID | FIELD_FLAG_HWUPDATED)
#define FIELD_SHADOWED_RW	(FIELD_FLAG_VALID | FIELD_FLAG_SHADOWED)
#define FIELD_SHADOWED_RO	(FIELD_FLAG_VALID | FIELD_FLAG_SHADOWED | \
				 FIELD_FLAG_READONLY)

struct vmcs_field_info {
	u16 offset;
	u8 flags;
};

/* By limiting the supported field indexes to 5 bits, we can build a more
 * compact encoding that still fits into 4K when using 64 bit values:
 *
 *  32 indexes * 4 types * 4 width * 8 bytes = 4096 bytes
 *
 * Furthermore, we limit the maximum index to 30 in order to have 8 bytes left
 * for the VMCS header. */
#define MAX_FIELD_INDEX		30
#define VCMS_COMPACT_INDEX_MASK	0x0000003e
#define VCMS_COMPACT_INDEX_BITS	5

#define VMCS_FIELD_TO_OFFSET(field)					\
	((((field) & VMCS_INDEX_MASK) >> VMCS_INDEX_SHIFT) |		\
	 (((field) & VMCS_TYPE_MASK) >>					\
	  (VMCS_TYPE_SHIFT - VCMS_COMPACT_INDEX_BITS)) |		\
	 (((field) & VMCS_WIDTH_MASK) >>				\
	  (VMCS_WIDTH_SHIFT - VCMS_COMPACT_INDEX_BITS - VMCS_TYPE_BITS)))

#define VMCS_OFFSET_TO_FIELD(offset)					\
	(((offset << VMCS_INDEX_SHIFT) & VCMS_COMPACT_INDEX_MASK) |	\
	 ((offset << (VMCS_TYPE_SHIFT - VCMS_COMPACT_INDEX_BITS)) &	\
	  VMCS_TYPE_MASK) |						\
	 ((offset <<							\
	   (VMCS_WIDTH_SHIFT - VCMS_COMPACT_INDEX_BITS - VMCS_TYPE_BITS)) & \
	   VMCS_WIDTH_MASK))

#define FIELD(flags, name) \
	[VMCS_FIELD_TO_OFFSET(name)] = flags

static const u8 vmcs_field_flags[] = {
	FIELD(FIELD_DIRECT_HWUPD, GUEST_ES_SELECTOR),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_CS_SELECTOR),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_SS_SELECTOR),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_DS_SELECTOR),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_FS_SELECTOR),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_GS_SELECTOR),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_LDTR_SELECTOR),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_TR_SELECTOR),
	FIELD(FIELD_SHADOWED_RW,  HOST_ES_SELECTOR),
	FIELD(FIELD_SHADOWED_RW,  HOST_CS_SELECTOR),
	FIELD(FIELD_SHADOWED_RW,  HOST_SS_SELECTOR),
	FIELD(FIELD_SHADOWED_RW,  HOST_DS_SELECTOR),
	FIELD(FIELD_SHADOWED_RW,  HOST_FS_SELECTOR),
	FIELD(FIELD_SHADOWED_RW,  HOST_GS_SELECTOR),
	FIELD(FIELD_SHADOWED_RW,  HOST_TR_SELECTOR),
	FIELD(FIELD_DIRECT,       IO_BITMAP_A),			// SHADOW!
	FIELD(FIELD_DIRECT,       IO_BITMAP_B),			// SHADOW!
	FIELD(FIELD_DIRECT,       MSR_BITMAP),			// CHECK/SHADOW!
	FIELD(FIELD_DIRECT,       VM_EXIT_MSR_LOAD_ADDR),	// SHADOW!
	FIELD(FIELD_DIRECT,       VM_ENTRY_MSR_LOAD_ADDR),	// SHADOW!
	FIELD(FIELD_DIRECT,       TSC_OFFSET),
	FIELD(FIELD_DIRECT,       VIRTUAL_APIC_PAGE_ADDR),	// CHECK!
	FIELD(FIELD_DIRECT,       APIC_ACCESS_ADDR),		// CHECK!
	FIELD(FIELD_DIRECT,       VMCS_LINK_POINTER),		// SHADOW!
	FIELD(FIELD_DIRECT_HWUPD, GUEST_IA32_DEBUGCTL),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_IA32_PAT),
	FIELD(FIELD_SHADOWED_RW,  HOST_IA32_PAT),
	FIELD(FIELD_DIRECT,       PIN_BASED_VM_EXEC_CONTROL),	// CHECK/SHADOW!
	FIELD(FIELD_DIRECT,       CPU_BASED_VM_EXEC_CONTROL),	// CHECK/SHADOW!
	FIELD(FIELD_DIRECT,       EXCEPTION_BITMAP),		// CHECK!
	FIELD(FIELD_DIRECT,       PAGE_FAULT_ERROR_CODE_MASK),
	FIELD(FIELD_DIRECT,       PAGE_FAULT_ERROR_CODE_MATCH),
	FIELD(FIELD_DIRECT,       CR3_TARGET_COUNT),
	FIELD(FIELD_DIRECT,       VM_EXIT_CONTROLS),		// CHECK/SHADOW!
	FIELD(FIELD_DIRECT,       VM_EXIT_MSR_STORE_COUNT),	// CHECK!
	FIELD(FIELD_DIRECT,       VM_EXIT_MSR_LOAD_COUNT),	// CHECK!
	FIELD(FIELD_DIRECT_HWUPD, VM_ENTRY_CONTROLS),		// CHECK/SHADOW!
	FIELD(FIELD_DIRECT,       VM_ENTRY_MSR_LOAD_COUNT),	// CHECK!
	FIELD(FIELD_DIRECT_HWUPD, VM_ENTRY_INTR_INFO_FIELD),
	FIELD(FIELD_DIRECT,       VM_ENTRY_EXCEPTION_ERROR_CODE),
	FIELD(FIELD_DIRECT,       VM_ENTRY_INSTRUCTION_LEN),
	FIELD(FIELD_DIRECT,       TPR_THRESHOLD),
	FIELD(FIELD_DIRECT,       SECONDARY_VM_EXEC_CONTROL),	// CHECK/SHADOW!
	FIELD(FIELD_SHADOWED_RO,  VM_INSTRUCTION_ERROR),
	FIELD(FIELD_SHADOWED_RO,  VM_EXIT_REASON),
	FIELD(FIELD_SHADOWED_RO,  VM_EXIT_INTR_INFO),
	FIELD(FIELD_SHADOWED_RO,  VM_EXIT_INTR_ERROR_CODE),
	FIELD(FIELD_SHADOWED_RO,  IDT_VECTORING_INFO_FIELD),
	FIELD(FIELD_SHADOWED_RO,  IDT_VECTORING_ERROR_CODE),
	FIELD(FIELD_SHADOWED_RO,  VM_EXIT_INSTRUCTION_LEN),
	FIELD(FIELD_SHADOWED_RO,  VMX_INSTRUCTION_INFO),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_ES_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_CS_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_SS_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_DS_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_FS_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_GS_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_LDTR_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_TR_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_GDTR_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_IDTR_LIMIT),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_ES_AR_BYTES),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_CS_AR_BYTES),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_SS_AR_BYTES),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_DS_AR_BYTES),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_FS_AR_BYTES),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_GS_AR_BYTES),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_LDTR_AR_BYTES),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_TR_AR_BYTES),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_INTERRUPTIBILITY_INFO),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_ACTIVITY_STATE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_SYSENTER_CS),
	FIELD(FIELD_SHADOWED_RW, HOST_IA32_SYSENTER_CS),
	FIELD(FIELD_DIRECT,       CR0_GUEST_HOST_MASK),
	FIELD(FIELD_DIRECT,       CR4_GUEST_HOST_MASK),
	FIELD(FIELD_DIRECT_HWUPD, CR0_READ_SHADOW),
	FIELD(FIELD_DIRECT_HWUPD, CR4_READ_SHADOW),
	FIELD(FIELD_SHADOWED_RO,  EXIT_QUALIFICATION),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_CR0),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_CR3),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_CR4),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_ES_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_CS_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_SS_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_DS_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_FS_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_GS_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_LDTR_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_TR_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_GDTR_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_IDTR_BASE),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_DR7),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_RSP),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_RIP),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_RFLAGS),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_PENDING_DBG_EXCEPTIONS),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_SYSENTER_ESP),
	FIELD(FIELD_DIRECT_HWUPD, GUEST_SYSENTER_EIP),
	FIELD(FIELD_SHADOWED_RW,  HOST_CR0),
	FIELD(FIELD_SHADOWED_RW,  HOST_CR3),
	FIELD(FIELD_SHADOWED_RW,  HOST_CR4),
	FIELD(FIELD_SHADOWED_RW,  HOST_FS_BASE),
	FIELD(FIELD_SHADOWED_RW,  HOST_GS_BASE),
	FIELD(FIELD_SHADOWED_RW,  HOST_TR_BASE),
	FIELD(FIELD_SHADOWED_RW,  HOST_GDTR_BASE),
	FIELD(FIELD_SHADOWED_RW,  HOST_IDTR_BASE),
	FIELD(FIELD_SHADOWED_RW,  HOST_IA32_SYSENTER_ESP),
	FIELD(FIELD_SHADOWED_RW,  HOST_IA32_SYSENTER_EIP),
	FIELD(FIELD_SHADOWED_RW,  HOST_RSP),
	FIELD(FIELD_SHADOWED_RW,  HOST_RIP),
};

static struct vmcs_field_info nvmx_vmcs_field_info(unsigned long field)
{
	unsigned long index = (field & VMCS_INDEX_MASK) >> VMCS_INDEX_SHIFT;
	struct vmcs_field_info field_info;

	if ((field & VMCS_HIGH_FIELD) || index > MAX_FIELD_INDEX) {
		field_info.offset = 0;
		field_info.flags = FIELD_INVALID;
	} else {
		field_info.offset = VMCS_FIELD_TO_OFFSET(field);
		field_info.flags = vmcs_field_flags[field_info.offset];
	}
	return field_info;
}

static bool nvmx_inject_exception_e(unsigned int exception,
				    unsigned int error_code)
{
	bool ok = true;

	ok &= vmcs_write32(VM_ENTRY_INTR_INFO_FIELD,
			   exception | INTR_TYPE_HARD_EXCEPTION |
			   INTR_INFO_DELIVER_CODE | INTR_INFO_VALID);
	ok &= vmcs_write32(VM_ENTRY_EXCEPTION_ERROR_CODE, error_code);
	return ok;
}

int nvmx_cpu_init(struct per_cpu *cpu_data)
{
	cpu_data->nested_vmcs.revision_id = cpu_data->vmcs.revision_id;
	cpu_data->nested_vmcs.shadow_indicator = 0;

	if (!vmcs_clear(&cpu_data->nested_vmcs) ||
	    !vmcs_load(&cpu_data->nested_vmcs) ||
	    !vmcs_host_setup() ||
	    !vmcs_load(&cpu_data->vmcs))
		return -EIO;

	cpu_data->mapped_nested_vmcs = page_alloc(&remap_pool, 1);
	if (!cpu_data->mapped_nested_vmcs)
		return -ENOMEM;

	return paging_create(&cpu_data->pg_structs, 0, PAGE_SIZE,
			     (unsigned long)cpu_data->mapped_nested_vmcs,
			     PAGE_DEFAULT_FLAGS, PAGE_DEFAULT_FLAGS);
}

void nvmx_cpu_exit(struct per_cpu *cpu_data)
{
	vmcs_clear(&cpu_data->nested_vmcs);
}

static bool nvmx_get_vmcs_address(u64 ptr, u64 *address)
{
	struct guest_paging_structures pg_structs;
	u8 *page;

	vcpu_get_guest_paging_structs(&pg_structs);

	page = paging_get_guest_pages(&pg_structs, ptr, 1, PAGE_DEFAULT_FLAGS);
	if (!page)
		return false;

	*address = *(u64 *)(page + (ptr & PAGE_OFFS_MASK));

	return true;
}

static bool nvmx_vmclear(struct per_cpu *cpu_data)
{
	u64 *vmcs_data = cpu_data->mapped_nested_vmcs->data;
	unsigned int n;
	u64 vmcs;

	// TODO: check if VMCS address is actually in RAX
	if (!nvmx_get_vmcs_address(cpu_data->guest_regs.rax, &vmcs))
		goto failed;

	if (vmcs == cpu_data->nested_vmcs_addr) {
		cpu_data->nested_vmcs_addr = -1;

		if (!vmcs_load(&cpu_data->nested_vmcs))
			goto failed;

		for (n = 0; n < ARRAY_SIZE(vmcs_field_flags); n++) {
			if (vmcs_field_flags[n] != FIELD_DIRECT_HWUPD)
				continue;
			vmcs_data[n] = vmcs_read64(VMCS_OFFSET_TO_FIELD(n));
		}

		vmcs_clear(&cpu_data->nested_vmcs);

		if (!vmcs_load(&cpu_data->vmcs))
			goto failed;
	}

	printk("VMCLEAR %llx\n", vmcs);

	vcpu_skip_emulated_instruction(X86_INST_LEN_VMCLEAR_RAX);
	return true;

failed:
	panic_printk("FATAL: VMCLEAR failed for %016llx\n", vmcs);
	return false;
}

static bool nvmx_vmptrld(struct per_cpu *cpu_data)
{
	u64 *vmcs_data = cpu_data->mapped_nested_vmcs->data;
	unsigned int n;
	u64 vmcs;

	// TODO: check if VMCS address is actually in RAX
	if (!nvmx_get_vmcs_address(cpu_data->guest_regs.rax, &vmcs))
		goto failed;

	// TODO: validate vmcs address and content
	cpu_data->nested_vmcs_addr = vmcs;
	if (paging_create(&cpu_data->pg_structs, vmcs, PAGE_SIZE,
			  (unsigned long)cpu_data->mapped_nested_vmcs,
			  PAGE_DEFAULT_FLAGS, PAGE_DEFAULT_FLAGS) < 0)
		goto failed;
	printk("current VMCS = %llx\n", vmcs);

	if (!vmcs_load(&cpu_data->nested_vmcs))
		goto failed;

	// TODO: validate content
	for (n = 0; n < ARRAY_SIZE(vmcs_field_flags); n++) {
		if ((vmcs_field_flags[n] &
		     (FIELD_FLAG_VALID | FIELD_FLAG_READONLY |
		      FIELD_FLAG_SHADOWED)) == FIELD_FLAG_VALID)
			if (!vmcs_write64(VMCS_OFFSET_TO_FIELD(n),
					  vmcs_data[n]))
				goto failed;
	}

	if (!vmcs_load(&cpu_data->vmcs))
		goto failed;

	vcpu_skip_emulated_instruction(X86_INST_LEN_VMPTRLD_RAX);
	return true;

failed:
	panic_printk("FATAL: VMPTRLD failed for %016llx\n", vmcs);
	return false;
}

static bool nvmx_vmwrite(struct per_cpu *cpu_data)
{
	u32 instr_info = vmcs_read32(VMX_INSTRUCTION_INFO);
	unsigned long field = cpu_data->guest_regs.rdx;
	struct vmcs_field_info field_info;
	u64 value;

	if (cpu_data->nested_vmcs_addr == -1) {
		panic_printk("FATAL: no current vmcs\n");
		return false;
	}

	switch (instr_info & VMREAD_VMWRITE_REG1_MASK) {
	case VMREAD_VMWRITE_REG1_RAX:
		value = cpu_data->guest_regs.rax;
		break;
	case VMREAD_VMWRITE_REG1_RSP:
		value = vmcs_read64(GUEST_RSP);
		break;
	default:
		panic_printk("FATAL: unsupported vmwrite source\n");
		return false;
	}
	if (!(instr_info & VMREAD_VMWRITE_REG2_VALID) ||
	    (instr_info & VMREAD_VMWRITE_REG2_MASK) !=
	    VMREAD_VMWRITE_REG2_RDX) {
		panic_printk("FATAL: unsupported vmwrite field register\n");
		return false;
	}

	field_info = nvmx_vmcs_field_info(field);
	if (!(field_info.flags & FIELD_FLAG_VALID)) {
		panic_printk("FATAL: unsupported vmwrite to %08lx\n", field);
		return false;
	}

	// TODO: validate access!

	if (!(field_info.flags & FIELD_FLAG_SHADOWED)) {
		if (!vmcs_load(&cpu_data->nested_vmcs))
			goto failed_switching;
		if (!vmcs_write64(field, value)) {
			panic_printk("FATAL: VMWRITE of %016llx at %08lx "
				     "failed\n", value, field);
			return false;
		}
		if (!vmcs_load(&cpu_data->vmcs))
			goto failed_switching;
	}
	cpu_data->mapped_nested_vmcs->data[field_info.offset] = value;
	//printk("VMWRITE: %016lx at %08lx\n", value, field);

	vcpu_skip_emulated_instruction(X86_INST_LEN_VMWRITE_REG_RDX);
	return true;

failed_switching:
	panic_printk("FATAL: Failed to switch between VMCS\n");
	return false;
}

static bool nvmx_vmread(struct per_cpu *cpu_data)
{
	u32 instr_info = vmcs_read32(VMX_INSTRUCTION_INFO);
	unsigned long field = cpu_data->guest_regs.rdx;
	struct vmcs_field_info field_info;
	u64 value;

	if (cpu_data->nested_vmcs_addr == -1) {
		panic_printk("FATAL: no current vmcs\n");
		return false;
	}

	if ((instr_info & VMREAD_VMWRITE_REG1_MASK) !=
	    VMREAD_VMWRITE_REG1_RAX) {
		panic_printk("FATAL: unsupported vmwrite source\n");
		return false;
	}
	if (!(instr_info & VMREAD_VMWRITE_REG2_VALID) ||
	    (instr_info & VMREAD_VMWRITE_REG2_MASK) !=
	    VMREAD_VMWRITE_REG2_RDX) {
		panic_printk("FATAL: unsupported vmwrite field register\n");
		return false;
	}

	field_info = nvmx_vmcs_field_info(field);
	if (!(field_info.flags & FIELD_FLAG_VALID)) {
		panic_printk("FATAL: unsupported vmread from %08lx\n", field);
		return false;
	}

	if (field_info.flags & FIELD_FLAG_SHADOWED)
		value = cpu_data->mapped_nested_vmcs->data[field_info.offset];
	else {
		if (!vmcs_load(&cpu_data->nested_vmcs))
			goto failed_switching;
		value = vmcs_read64(field);
		if (!vmcs_load(&cpu_data->vmcs))
			goto failed_switching;
	}
	//printk("VMREAD:  %016lx at %08lx\n", value, field);

	cpu_data->guest_regs.rax = value;
	vcpu_skip_emulated_instruction(X86_INST_LEN_VMREAD_RAX_RDX);
	return true;

failed_switching:
	panic_printk("FATAL: Failed to switch between VMCS\n");
	return false;
}

static bool nvmx_run(struct per_cpu *cpu_data, bool launch)
{
	//printk("VMRUN launch=%d\n", launch);

	if (cpu_data->nested)
		return false;

	cpu_data->nested = true;
	// barrier()

	if (!vmcs_load(&cpu_data->nested_vmcs)) {
		panic_printk("FATAL: Failed to switch to nested VMCS\n");
		return false;
	}

	if (!launch)
		return true;

	asm volatile(
		"push %%rbp\n\t"
		"mov (%%rdi),%%r15\n\t"
		"mov 0x8(%%rdi),%%r14\n\t"
		"mov 0x10(%%rdi),%%r13\n\t"
		"mov 0x18(%%rdi),%%r12\n\t"
		"mov 0x20(%%rdi),%%r11\n\t"
		"mov 0x28(%%rdi),%%r10\n\t"
		"mov 0x30(%%rdi),%%r9\n\t"
		"mov 0x38(%%rdi),%%r8\n\t"
		"mov 0x48(%%rdi),%%rsi\n\t"
		"mov 0x50(%%rdi),%%rbp\n\t"
		"mov 0x60(%%rdi),%%rbx\n\t"
		"mov 0x68(%%rdi),%%rdx\n\t"
		"mov 0x70(%%rdi),%%rcx\n\t"
		"mov 0x78(%%rdi),%%rax\n\t"
		"mov 0x40(%%rdi),%%rdi\n\t"
		"vmlaunch\n\t"
		"pop %%rbp"
		: /* no output */
		: "D" (&cpu_data->guest_regs)
		: "memory", "r15", "r14", "r13", "r12", "r11", "r10", "r9",
		  "r8", "rsi", "rbx", "rdx", "rcx", "rax", "cc");

	panic_printk("FATAL: nested vmlaunch failed, error %d\n",
		     vmcs_read32(VM_INSTRUCTION_ERROR));
	panic_stop();
}

static bool nvmx_exit(u32 reason, struct per_cpu *cpu_data)
{
	u64 *vmcs_data = cpu_data->mapped_nested_vmcs->data;
	unsigned int n;

	//printk("VMEXIT, reason=%d\n", reason);

	for (n = 0; n < ARRAY_SIZE(vmcs_field_flags); n++) {
		if (vmcs_field_flags[n] != FIELD_SHADOWED_RO)
			continue;
		vmcs_data[n] = vmcs_read64(VMCS_OFFSET_TO_FIELD(n));
	}

	cpu_data->nested = false;
	// wmb()

	if (!vmcs_load(&cpu_data->vmcs) ||
	    !vmcs_write64(GUEST_RSP,
			  vmcs_data[VMCS_FIELD_TO_OFFSET(HOST_RSP)]) ||
	    !vmcs_write64(GUEST_RIP,
			  vmcs_data[VMCS_FIELD_TO_OFFSET(HOST_RIP)])) {
		panic_printk("FATAL: Failed nested VM exit\n");
		return false;
	}

	return true;
}

enum nvmx_return nvmx_handle_exit(u32 reason, struct per_cpu *cpu_data)
{
	if (cpu_data->nested) {
		if (!nvmx_exit(reason, cpu_data))
			return NVMX_FAILED;
		goto vmsucceed;
	}

	switch (reason) {
	case EXIT_REASON_VMWRITE:
		if (!nvmx_vmwrite(cpu_data))
			return NVMX_FAILED;
		break;
	case EXIT_REASON_VMREAD:
		if (!nvmx_vmread(cpu_data))
			return NVMX_FAILED;
		break;
	case EXIT_REASON_VMLAUNCH:
	case EXIT_REASON_VMRESUME:
		if (!nvmx_run(cpu_data, reason == EXIT_REASON_VMLAUNCH))
			return NVMX_FAILED;
		return NVMX_HANDLED;
	case EXIT_REASON_VMCLEAR:
		if (!nvmx_vmclear(cpu_data))
			return NVMX_FAILED;
		break;
	case EXIT_REASON_VMPTRLD:
		if (!nvmx_vmptrld(cpu_data))
			return NVMX_FAILED;
		break;
	case EXIT_REASON_VMXON:
		cpu_data->nested_vmcs_addr = -1;
		vcpu_skip_emulated_instruction(X86_INST_LEN_VMXON_RAX);
		break;
	case EXIT_REASON_VMXOFF:
		cpu_data->nested_vmcs_addr = -1;
		vcpu_skip_emulated_instruction(X86_INST_LEN_VMXOFF);
		break;
	case EXIT_REASON_INVEPT:
		vcpu_skip_emulated_instruction(X86_INST_LEN_INVEPT);
		break;
	case EXIT_REASON_MSR_READ:
		switch (cpu_data->guest_regs.rcx) {
		case MSR_VM_HSAVE_PA:
			return nvmx_inject_exception_e(GP_VECTOR, 0) ?
				NVMX_HANDLED : NVMX_FAILED;
		default:
			return NVMX_UNHANDLED;
		}
	default:
		return NVMX_UNHANDLED;
	}

vmsucceed:
	vmcs_write64(GUEST_RFLAGS, vmcs_read64(GUEST_RFLAGS) &
		     ~(X86_EFLAGS_CF | X86_EFLAGS_PF | X86_EFLAGS_AF |
		       X86_EFLAGS_ZF | X86_EFLAGS_SF | X86_EFLAGS_OF));
	return NVMX_HANDLED;
}
