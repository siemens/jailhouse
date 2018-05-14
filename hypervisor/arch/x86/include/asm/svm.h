/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2005-2007, Advanced Micro Devices, Inc
 * Copyright (c) 2004, Intel Corporation.
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This file is partially derived from
 * xvisor/arch/x86/cpu/x86_64/include/vm/amd_vmcb.h, which comes with
 * Xvisor 0.2 (http://xhypervisor.org).
 *
 * Copyright (c) 2005-2007, Advanced Micro Devices, Inc
 * Copyright (c) 2004, Intel Corporation.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_SVM_H
#define _JAILHOUSE_ASM_SVM_H

#include <jailhouse/types.h>

#define EFER_SVME		(1UL << 12)
#define VM_CR_SVMDIS		(1UL << 4)

#define MSR_VM_CR		0xc0010114
#define MSR_VM_HSAVE_PA		0xc0010117

#define SVM_MSRPM_0000		0
#define SVM_MSRPM_C000		1
#define SVM_MSRPM_C001		2
#define SVM_MSRPM_RESV		3

#define SVM_TLB_FLUSH_ALL	0x01
#define SVM_TLB_FLUSH_GUEST	0x03

#define SVM_EVENTINJ_EXCEPTION	(3UL << 8)
#define SVM_EVENTINJ_ERR_VALID	(1UL << 11)
#define SVM_EVENTINJ_VALID	(1UL << 31)

struct svm_segment {
	u16 selector;
	u16 attributes;
	u32 limit;
	u64 base;
} __attribute__((packed));

/* general 1 intercepts */
enum generic_interrupt_1_bits {
	GENERAL1_INTERCEPT_INTR		 = 1 << 0,
	GENERAL1_INTERCEPT_NMI		 = 1 << 1,
	GENERAL1_INTERCEPT_SMI		 = 1 << 2,
	GENERAL1_INTERCEPT_INIT		 = 1 << 3,
	GENERAL1_INTERCEPT_VINTR	 = 1 << 4,
	GENERAL1_INTERCEPT_CR0_SEL_WRITE = 1 << 5,
	GENERAL1_INTERCEPT_IDTR_READ	 = 1 << 6,
	GENERAL1_INTERCEPT_GDTR_READ	 = 1 << 7,
	GENERAL1_INTERCEPT_LDTR_READ	 = 1 << 8,
	GENERAL1_INTERCEPT_TR_READ	 = 1 << 9,
	GENERAL1_INTERCEPT_IDTR_WRITE	 = 1 << 10,
	GENERAL1_INTERCEPT_GDTR_WRITE	 = 1 << 11,
	GENERAL1_INTERCEPT_LDTR_WRITE	 = 1 << 12,
	GENERAL1_INTERCEPT_TR_WRITE	 = 1 << 13,
	GENERAL1_INTERCEPT_RDTSC	 = 1 << 14,
	GENERAL1_INTERCEPT_RDPMC	 = 1 << 15,
	GENERAL1_INTERCEPT_PUSHF	 = 1 << 16,
	GENERAL1_INTERCEPT_POPF		 = 1 << 17,
	GENERAL1_INTERCEPT_CPUID	 = 1 << 18,
	GENERAL1_INTERCEPT_RSM		 = 1 << 19,
	GENERAL1_INTERCEPT_IRET		 = 1 << 20,
	GENERAL1_INTERCEPT_SWINT	 = 1 << 21,
	GENERAL1_INTERCEPT_INVD		 = 1 << 22,
	GENERAL1_INTERCEPT_PAUSE	 = 1 << 23,
	GENERAL1_INTERCEPT_HLT		 = 1 << 24,
	GENERAL1_INTERCEPT_INVLPG	 = 1 << 25,
	GENERAL1_INTERCEPT_INVLPGA	 = 1 << 26,
	GENERAL1_INTERCEPT_IOIO_PROT	 = 1 << 27,
	GENERAL1_INTERCEPT_MSR_PROT	 = 1 << 28,
	GENERAL1_INTERCEPT_TASK_SWITCH	 = 1 << 29,
	GENERAL1_INTERCEPT_FERR_FREEZE	 = 1 << 30,
	GENERAL1_INTERCEPT_SHUTDOWN_EVT	 = 1 << 31
};

/* general 2 intercepts */
enum generic_interrupts_2_bits {
	GENERAL2_INTERCEPT_VMRUN   = 1 << 0,
	GENERAL2_INTERCEPT_VMMCALL = 1 << 1,
	GENERAL2_INTERCEPT_VMLOAD  = 1 << 2,
	GENERAL2_INTERCEPT_VMSAVE  = 1 << 3,
	GENERAL2_INTERCEPT_STGI	   = 1 << 4,
	GENERAL2_INTERCEPT_CLGI	   = 1 << 5,
	GENERAL2_INTERCEPT_SKINIT  = 1 << 6,
	GENERAL2_INTERCEPT_RDTSCP  = 1 << 7,
	GENERAL2_INTERCEPT_ICEBP   = 1 << 8,
	GENERAL2_INTERCEPT_WBINVD  = 1 << 9,
	GENERAL2_INTERCEPT_MONITOR = 1 << 10,
	GENERAL2_INTERCEPT_MWAIT   = 1 << 11,
	GENERAL2_INTERCEPT_MWAIT_CONDITIONAL = 1 << 12
};

enum vm_exit_code {
	/* control register read exitcodes */
	VMEXIT_CR0_READ	   =   0,
	VMEXIT_CR1_READ	   =   1,
	VMEXIT_CR2_READ	   =   2,
	VMEXIT_CR3_READ	   =   3,
	VMEXIT_CR4_READ	   =   4,
	VMEXIT_CR5_READ	   =   5,
	VMEXIT_CR6_READ	   =   6,
	VMEXIT_CR7_READ	   =   7,
	VMEXIT_CR8_READ	   =   8,
	VMEXIT_CR9_READ	   =   9,
	VMEXIT_CR10_READ   =  10,
	VMEXIT_CR11_READ   =  11,
	VMEXIT_CR12_READ   =  12,
	VMEXIT_CR13_READ   =  13,
	VMEXIT_CR14_READ   =  14,
	VMEXIT_CR15_READ   =  15,

	/* control register write exitcodes */
	VMEXIT_CR0_WRITE   =  16,
	VMEXIT_CR1_WRITE   =  17,
	VMEXIT_CR2_WRITE   =  18,
	VMEXIT_CR3_WRITE   =  19,
	VMEXIT_CR4_WRITE   =  20,
	VMEXIT_CR5_WRITE   =  21,
	VMEXIT_CR6_WRITE   =  22,
	VMEXIT_CR7_WRITE   =  23,
	VMEXIT_CR8_WRITE   =  24,
	VMEXIT_CR9_WRITE   =  25,
	VMEXIT_CR10_WRITE  =  26,
	VMEXIT_CR11_WRITE  =  27,
	VMEXIT_CR12_WRITE  =  28,
	VMEXIT_CR13_WRITE  =  29,
	VMEXIT_CR14_WRITE  =  30,
	VMEXIT_CR15_WRITE  =  31,

	/* debug register read exitcodes */
	VMEXIT_DR0_READ	   =  32,
	VMEXIT_DR1_READ	   =  33,
	VMEXIT_DR2_READ	   =  34,
	VMEXIT_DR3_READ	   =  35,
	VMEXIT_DR4_READ	   =  36,
	VMEXIT_DR5_READ	   =  37,
	VMEXIT_DR6_READ	   =  38,
	VMEXIT_DR7_READ	   =  39,
	VMEXIT_DR8_READ	   =  40,
	VMEXIT_DR9_READ	   =  41,
	VMEXIT_DR10_READ   =  42,
	VMEXIT_DR11_READ   =  43,
	VMEXIT_DR12_READ   =  44,
	VMEXIT_DR13_READ   =  45,
	VMEXIT_DR14_READ   =  46,
	VMEXIT_DR15_READ   =  47,

	/* debug register write exitcodes */
	VMEXIT_DR0_WRITE   =  48,
	VMEXIT_DR1_WRITE   =  49,
	VMEXIT_DR2_WRITE   =  50,
	VMEXIT_DR3_WRITE   =  51,
	VMEXIT_DR4_WRITE   =  52,
	VMEXIT_DR5_WRITE   =  53,
	VMEXIT_DR6_WRITE   =  54,
	VMEXIT_DR7_WRITE   =  55,
	VMEXIT_DR8_WRITE   =  56,
	VMEXIT_DR9_WRITE   =  57,
	VMEXIT_DR10_WRITE  =  58,
	VMEXIT_DR11_WRITE  =  59,
	VMEXIT_DR12_WRITE  =  60,
	VMEXIT_DR13_WRITE  =  61,
	VMEXIT_DR14_WRITE  =  62,
	VMEXIT_DR15_WRITE  =  63,

	/* processor exception exitcodes (VMEXIT_EXCP[0-31]) */
	VMEXIT_EXCEPTION_DE	 =	64, /* divide-by-zero-error */
	VMEXIT_EXCEPTION_DB	 =	65, /* debug */
	VMEXIT_EXCEPTION_NMI	 =	66, /* non-maskable-interrupt */
	VMEXIT_EXCEPTION_BP	 =	67, /* breakpoint */
	VMEXIT_EXCEPTION_OF	 =	68, /* overflow */
	VMEXIT_EXCEPTION_BR	 =	69, /* bound-range */
	VMEXIT_EXCEPTION_UD	 =	70, /* invalid-opcode*/
	VMEXIT_EXCEPTION_NM	 =	71, /* device-not-available */
	VMEXIT_EXCEPTION_DF	 =	72, /* double-fault */
	VMEXIT_EXCEPTION_09	 =	73, /* unsupported (reserved) */
	VMEXIT_EXCEPTION_TS	 =	74, /* invalid-tss */
	VMEXIT_EXCEPTION_NP	 =	75, /* segment-not-present */
	VMEXIT_EXCEPTION_SS	 =	76, /* stack */
	VMEXIT_EXCEPTION_GP	 =	77, /* general-protection */
	VMEXIT_EXCEPTION_PF	 =	78, /* page-fault */
	VMEXIT_EXCEPTION_15	 =	79, /* reserved */
	VMEXIT_EXCEPTION_MF	 =	80, /* x87 floating-point exception-pending */
	VMEXIT_EXCEPTION_AC	 =	81, /* alignment-check */
	VMEXIT_EXCEPTION_MC	 =	82, /* machine-check */
	VMEXIT_EXCEPTION_XF	 =	83, /* simd floating-point */

	/* exceptions 20-31 (exitcodes 84-95) are reserved */

	/* ...and the rest of the #VMEXITs */
	VMEXIT_INTR			=  96,
	VMEXIT_NMI			=  97,
	VMEXIT_SMI			=  98,
	VMEXIT_INIT			=  99,
	VMEXIT_VINTR			= 100,
	VMEXIT_CR0_SEL_WRITE		= 101,
	VMEXIT_IDTR_READ		= 102,
	VMEXIT_GDTR_READ		= 103,
	VMEXIT_LDTR_READ		= 104,
	VMEXIT_TR_READ			= 105,
	VMEXIT_IDTR_WRITE		= 106,
	VMEXIT_GDTR_WRITE		= 107,
	VMEXIT_LDTR_WRITE		= 108,
	VMEXIT_TR_WRITE			= 109,
	VMEXIT_RDTSC			= 110,
	VMEXIT_RDPMC			= 111,
	VMEXIT_PUSHF			= 112,
	VMEXIT_POPF			= 113,
	VMEXIT_CPUID			= 114,
	VMEXIT_RSM			= 115,
	VMEXIT_IRET			= 116,
	VMEXIT_SWINT			= 117,
	VMEXIT_INVD			= 118,
	VMEXIT_PAUSE			= 119,
	VMEXIT_HLT			= 120,
	VMEXIT_INVLPG			= 121,
	VMEXIT_INVLPGA			= 122,
	VMEXIT_IOIO			= 123,
	VMEXIT_MSR			= 124,
	VMEXIT_TASK_SWITCH		= 125,
	VMEXIT_FERR_FREEZE		= 126,
	VMEXIT_SHUTDOWN			= 127,
	VMEXIT_VMRUN			= 128,
	VMEXIT_VMMCALL			= 129,
	VMEXIT_VMLOAD			= 130,
	VMEXIT_VMSAVE			= 131,
	VMEXIT_STGI			= 132,
	VMEXIT_CLGI			= 133,
	VMEXIT_SKINIT			= 134,
	VMEXIT_RDTSCP			= 135,
	VMEXIT_ICEBP			= 136,
	VMEXIT_WBINVD			= 137,
	VMEXIT_MONITOR			= 138,
	VMEXIT_MWAIT			= 139,
	VMEXIT_MWAIT_CONDITIONAL	= 140,
	VMEXIT_XSETBV                   = 141,
	VMEXIT_NPF			= 1024, /* nested paging fault */
	VMEXIT_INVALID			=  -1
};

enum clean_bits {
	CLEAN_BITS_I	= 1 << 0,
	CLEAN_BITS_IOPM	= 1 << 1,
	CLEAN_BITS_ASID	= 1 << 2,
	CLEAN_BITS_TPR	= 1 << 3,
	CLEAN_BITS_NP	= 1 << 4,
	CLEAN_BITS_CRX	= 1 << 5,
	CLEAN_BITS_DRX	= 1 << 6,
	CLEAN_BITS_DT	= 1 << 7,
	CLEAN_BITS_SEG	= 1 << 8,
	CLEAN_BITS_CR2	= 1 << 9,
	CLEAN_BITS_LBR	= 1 << 10,
	CLEAN_BITS_AVIC	= 1 << 11
};

typedef u64 vintr_t;
typedef u64 lbrctrl_t;

struct vmcb {
	u32 cr_intercepts;		/* offset 0x00 */
	u32 dr_intercepts;		/* offset 0x04 */
	u32 exception_intercepts;	/* offset 0x08 */
	u32 general1_intercepts;	/* offset 0x0C */
	u32 general2_intercepts;	/* offset 0x10 */
	u32 res01;			/* offset 0x14 */
	u64 res02;			/* offset 0x18 */
	u64 res03;			/* offset 0x20 */
	u64 res04;			/* offset 0x28 */
	u64 res05;			/* offset 0x30 */
	u32 res06;			/* offset 0x38 */
	u16 res06a;			/* offset 0x3C */
	u16 pause_filter_count;		/* offset 0x3E */
	u64 iopm_base_pa;		/* offset 0x40 */
	u64 msrpm_base_pa;		/* offset 0x48 */
	u64 tsc_offset;			/* offset 0x50 */
	u32 guest_asid;			/* offset 0x58 */
	u8 tlb_control;			/* offset 0x5C */
	u8 res07[3];
	vintr_t vintr;			/* offset 0x60 */
	u64 interrupt_shadow;		/* offset 0x68 */
	u64 exitcode;			/* offset 0x70 */
	u64 exitinfo1;			/* offset 0x78 */
	u64 exitinfo2;			/* offset 0x80 */
	u64 exitintinfo;		/* offset 0x88 */
	u64 np_enable;			/* offset 0x90 */
	u64 res08[2];
	u32 eventinj;			/* offset 0xA8 */
	u32 eventinj_err;		/* offset 0xAC */
	u64 n_cr3;			/* offset 0xB0 */
	lbrctrl_t lbr_control;		/* offset 0xB8 */
	u64 clean_bits;			/* offset 0xC0 */
	u64 nextrip;			/* offset 0xC8 */
	u8 bytes_fetched;		/* offset 0xD0 */
	u8 guest_bytes[15];
	u64 res10a[100];		/* offset 0xE0 pad to save area */

	struct svm_segment es;		/* offset 1024 */
	struct svm_segment cs;
	struct svm_segment ss;
	struct svm_segment ds;
	struct svm_segment fs;
	struct svm_segment gs;
	struct svm_segment gdtr;
	struct svm_segment ldtr;
	struct svm_segment idtr;
	struct svm_segment tr;

	u64 res10[5];
	u8 res11[3];
	u8 cpl;
	u32 res12;
	u64 efer;			/* offset 1024 + 0xD0 */
	u64 res13[14];
	u64 cr4;			/* loffset 1024 + 0x148 */
	u64 cr3;
	u64 cr0;
	u64 dr7;
	u64 dr6;
	u64 rflags;
	u64 rip;
	u64 res14[11];
	u64 rsp;
	u64 res15[3];
	u64 rax;
	u64 star;
	u64 lstar;
	u64 cstar;
	u64 sfmask;
	u64 kerngsbase;
	u64 sysenter_cs;
	u64 sysenter_esp;
	u64 sysenter_eip;
	u64 cr2;
	u8 reserved[32];
	u64 g_pat;
	u64 debugctlmsr;
	u64 lastbranchfromip;
	u64 lastbranchtoip;
	u64 lastintfromip;
	u64 lastinttoip;
	u64 res16[301];
} __attribute__((packed));

#endif
