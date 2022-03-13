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
 *
 * This file is based on linux/arch/x86/include/asm/special_insn.h and other
 * kernel headers:
 *
 * Copyright (c) Linux kernel developers, 2013
 */

#ifndef _JAILHOUSE_ASM_PROCESSOR_H
#define _JAILHOUSE_ASM_PROCESSOR_H

#include <jailhouse/types.h>

/* leaf 0x01, ECX */
#define X86_FEATURE_VMX					(1 << 5)
#define X86_FEATURE_XSAVE				(1 << 26)
#define X86_FEATURE_OSXSAVE				(1 << 27)
#define X86_FEATURE_HYPERVISOR				(1 << 31)

/* leaf 0x07, subleaf 0, EBX */
#define X86_FEATURE_INVPCID				(1 << 10)
#define X86_FEATURE_CAT					(1 << 15)

/* leaf 0x07, subleaf 0, ECX */
#define X86_FEATURE_WAITPKG				(1 << 5)

/* leaf 0x0d, subleaf 1, EAX */
#define X86_FEATURE_XSAVES				(1 << 3)

/* leaf 0x80000001, ECX */
#define X86_FEATURE_SVM					(1 << 2)

/* leaf 0x80000001, EDX */
#define X86_FEATURE_GBPAGES				(1 << 26)
#define X86_FEATURE_RDTSCP				(1 << 27)

/* leaf 0x8000000a, EDX */
#define X86_FEATURE_NP					(1 << 0)
#define X86_FEATURE_FLUSH_BY_ASID			(1 << 6)
#define X86_FEATURE_DECODE_ASSISTS			(1 << 7)
#define X86_FEATURE_AVIC				(1 << 13)

#define X86_RFLAGS_VM					(1 << 17)

#define X86_CR0_PE					(1UL << 0)
#define X86_CR0_MP					(1UL << 1)
#define X86_CR0_TS					(1UL << 3)
#define X86_CR0_ET					(1UL << 4)
#define X86_CR0_NE					(1UL << 5)
#define X86_CR0_WP					(1UL << 16)
#define X86_CR0_NW					(1UL << 29)
#define X86_CR0_CD					(1UL << 30)
#define X86_CR0_PG					(1UL << 31)
#define X86_CR0_RESERVED				\
	(BIT_MASK(28, 19) |  (1UL << 17) | BIT_MASK(15, 6))

#define X86_CR4_PAE					(1UL << 5)
#define X86_CR4_VMXE					(1UL << 13)
#define X86_CR4_OSXSAVE					(1UL << 18)
#define X86_CR4_RESERVED				\
	(BIT_MASK(31, 23) | (1UL << 19) | (1UL << 15) | (1UL << 12))

#define X86_XCR0_FP					0x00000001

#define MSR_IA32_APICBASE				0x0000001b
#define MSR_IA32_FEATURE_CONTROL			0x0000003a
#define MSR_IA32_PAT					0x00000277
#define MSR_IA32_MTRR_DEF_TYPE				0x000002ff
#define MSR_IA32_SYSENTER_CS				0x00000174
#define MSR_IA32_SYSENTER_ESP				0x00000175
#define MSR_IA32_SYSENTER_EIP				0x00000176
#define MSR_IA32_PERF_GLOBAL_CTRL			0x0000038f
#define MSR_IA32_VMX_BASIC				0x00000480
#define MSR_IA32_VMX_PINBASED_CTLS			0x00000481
#define MSR_IA32_VMX_PROCBASED_CTLS			0x00000482
#define MSR_IA32_VMX_EXIT_CTLS				0x00000483
#define MSR_IA32_VMX_ENTRY_CTLS				0x00000484
#define MSR_IA32_VMX_MISC				0x00000485
#define MSR_IA32_VMX_CR0_FIXED0				0x00000486
#define MSR_IA32_VMX_CR0_FIXED1				0x00000487
#define MSR_IA32_VMX_CR4_FIXED0				0x00000488
#define MSR_IA32_VMX_CR4_FIXED1				0x00000489
#define MSR_IA32_VMX_PROCBASED_CTLS2			0x0000048b
#define MSR_IA32_VMX_EPT_VPID_CAP			0x0000048c
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS		0x0000048e
#define MSR_X2APIC_BASE					0x00000800
#define MSR_X2APIC_ICR					0x00000830
#define MSR_X2APIC_END					0x0000083f
#define MSR_IA32_PQR_ASSOC				0x00000c8f
#define MSR_IA32_L3_MASK_0				0x00000c90
#define MSR_EFER					0xc0000080
#define MSR_STAR					0xc0000081
#define MSR_LSTAR					0xc0000082
#define MSR_CSTAR					0xc0000083
#define MSR_SFMASK					0xc0000084
#define MSR_FS_BASE					0xc0000100
#define MSR_GS_BASE					0xc0000101
#define MSR_KERNGS_BASE					0xc0000102

#define FEATURE_CONTROL_LOCKED				(1 << 0)
#define FEATURE_CONTROL_VMXON_ENABLED_OUTSIDE_SMX	(1 << 2)

#define PAT_RESET_VALUE					0x0007040600070406UL
/* PAT0: WB, PAT1: WC, PAT2: UC- */
#define PAT_HOST_VALUE					0x070106UL

#define MTRR_ENABLE					(1UL << 11)

#define EFER_LME					0x00000100
#define EFER_LMA					0x00000400
#define EFER_NXE					0x00000800

#define PQR_ASSOC_COS_SHIFT				32

#define CAT_RESID_L3					1

#define CAT_CBM_LEN_MASK				BIT_MASK(4, 0)
#define CAT_COS_MAX_MASK				BIT_MASK(15, 0)

#define GDT_DESC_NULL					0
#define GDT_DESC_CODE					1
#define GDT_DESC_TSS					2
#define GDT_DESC_TSS_HI					3
/*
 * Linux uses 16 entries, we only 4. But we need to be able to reload the Linux
 * TSS from our GDT because Linux write-protects its GDT. So, leave some space.
 */
#define NUM_GDT_DESC					16

#define X86_INST_LEN_CPUID				2
#define X86_INST_LEN_RDMSR				2
#define X86_INST_LEN_WRMSR				2
/* This covers both VMCALL and VMMCALL */
#define X86_INST_LEN_HYPERCALL				3
#define X86_INST_LEN_MOV_TO_CR				3
#define X86_INST_LEN_XSETBV				3

#define X86_REX_CODE					4

#define X86_PREFIX_OP_SZ				0x66
#define X86_PREFIX_ADDR_SZ				0x67

#define X86_OP_MOVZX_OPC1				0x0f
#define X86_OP_MOVZX_OPC2_B				0xb6
#define X86_OP_MOVZX_OPC2_W				0xb7
#define X86_OP_MOVB_TO_MEM				0x88
#define X86_OP_MOV_TO_MEM				0x89
#define X86_OP_MOVB_FROM_MEM				0x8a
#define X86_OP_MOV_FROM_MEM				0x8b
#define X86_OP_MOV_IMMEDIATE_TO_MEM			0xc7
#define X86_OP_MOV_MEM_TO_AX    			0xa1
#define X86_OP_MOV_AX_TO_MEM				0xa3

#define DB_VECTOR					1
#define NMI_VECTOR					2
#define PF_VECTOR					14
#define AC_VECTOR					17

#define EXCEPTION_NO_ERROR				0xffffffffffffffff

#define DESC_TSS_BUSY					(1UL << (9 + 32))
#define DESC_PRESENT					(1UL << (15 + 32))
#define DESC_CODE_DATA					(1UL << (12 + 32))
#define DESC_PAGE_GRAN					(1UL << (23 + 32))

#ifndef __ASSEMBLY__

/**
 * @ingroup X86
 * @defgroup Processor Processor
 *
 * Low-level support for x86 processor configuration and status retrieval.
 *
 * @{
 */

union registers {
	struct {
		unsigned long r15;
		unsigned long r14;
		unsigned long r13;
		unsigned long r12;
		unsigned long r11;
		unsigned long r10;
		unsigned long r9;
		unsigned long r8;
		unsigned long rdi;
		unsigned long rsi;
		unsigned long rbp;
		unsigned long unused;
		unsigned long rbx;
		unsigned long rdx;
		unsigned long rcx;
		unsigned long rax;
	};
	unsigned long by_index[16];
};

struct desc_table_reg {
	u16 limit;
	u64 base;
} __attribute__((packed));

struct segment {
	u64 base;
	u32 limit;
	u32 access_rights;
	u16 selector;
};

static unsigned long __force_order;

static inline void cpu_relax(void)
{
	asm volatile("rep; nop" : : : "memory");
}

static inline void memory_barrier(void)
{
	asm volatile("mfence" : : : "memory");
}

static inline void memory_load_barrier(void)
{
	asm volatile("lfence" : : : "memory");
}

static inline void cpuid(unsigned int *eax, unsigned int *ebx,
			 unsigned int *ecx, unsigned int *edx)
{
	/* ecx is often an input as well as an output. */
	asm volatile("cpuid"
	    : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
	    : "0" (*eax), "2" (*ecx)
	    : "memory");
}

#define CPUID_REG(reg)							\
static inline unsigned int cpuid_##reg(unsigned int op, unsigned int sub) \
{									\
	unsigned int eax, ebx, ecx, edx;				\
									\
	eax = op;							\
	ecx = sub;							\
	cpuid(&eax, &ebx, &ecx, &edx);					\
	return reg;							\
}

CPUID_REG(eax)
CPUID_REG(ebx)
CPUID_REG(ecx)
CPUID_REG(edx)

static inline unsigned long read_cr0(void)
{
	unsigned long cr0;

	asm volatile("mov %%cr0,%0" : "=r" (cr0), "=m" (__force_order));
	return cr0;
}

static inline void write_cr0(unsigned long val)
{
	asm volatile("mov %0,%%cr0" : : "r" (val), "m" (__force_order));
}

static inline unsigned long read_cr2(void)
{
	unsigned long cr2;

	asm volatile("mov %%cr2,%0" : "=r" (cr2), "=m" (__force_order));
	return cr2;
}

static inline unsigned long read_cr3(void)
{
	unsigned long cr3;

	asm volatile("mov %%cr3,%0" : "=r" (cr3), "=m" (__force_order));
	return cr3;
}

static inline void write_cr3(unsigned long val)
{
	asm volatile("mov %0,%%cr3" : : "r" (val), "m" (__force_order));
}

static inline unsigned long read_cr4(void)
{
	unsigned long cr4;

	asm volatile("mov %%cr4,%0" : "=r" (cr4), "=m" (__force_order));
	return cr4;
}

static inline void write_cr4(unsigned long val)
{
	asm volatile("mov %0,%%cr4" : : "r" (val), "m" (__force_order));
}

static inline unsigned long read_msr(unsigned int msr)
{
	u32 low, high;

	asm volatile("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
	return low | ((unsigned long)high << 32);
}

static inline void write_msr(unsigned int msr, unsigned long val)
{
	asm volatile("wrmsr"
		: /* no output */
		: "c" (msr), "a" (val), "d" (val >> 32)
		: "memory");
}

static inline void set_rdmsr_value(union registers *regs, unsigned long val)
{
	regs->rax = (u32)val;
	regs->rdx = val >> 32;
}

static inline unsigned long get_wrmsr_value(union registers *regs)
{
	return (u32)regs->rax | (regs->rdx << 32);
}

static inline void read_gdtr(struct desc_table_reg *val)
{
	asm volatile("sgdtq %0" : "=m" (*val));
}

static inline void write_gdtr(struct desc_table_reg *val)
{
	asm volatile("lgdtq %0" : : "m" (*val));
}

static inline void read_idtr(struct desc_table_reg *val)
{
	asm volatile("sidtq %0" : "=m" (*val));
}

static inline void write_idtr(struct desc_table_reg *val)
{
	asm volatile("lidtq %0" : : "m" (*val));
}

/**
 * Enable or disable interrupts delivery to the local CPU when in host mode.
 *
 * In some cases (AMD) changing IF isn't enough, so these are implemented on
 * per-vendor basis.
 * @{
 */
void enable_irq(void);

void disable_irq(void);
/** @} */

/** @} */
#endif /* !__ASSEMBLY__ */

#endif /* !_JAILHOUSE_ASM_PROCESSOR_H */
