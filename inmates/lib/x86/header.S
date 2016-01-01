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

#include <inmate.h>

#define X86_CR0_PE		0x00000001
#define X86_CR0_WP		0x00010000
#define X86_CR0_PG		0x80000000

#define X86_CR4_PAE		0x00000020

#define MSR_EFER		0xc0000080
#define EFER_LME		0x00000100

#define MSR_MTRR_DEF_TYPE	0x000002ff
#define MTRR_ENABLE		0x00000800

	.code16
	.section ".boot", "ax"

	.globl __reset_entry
__reset_entry:
	ljmp $0xf000,$start16


	.section ".startup", "ax"

start16:
	lgdtl %cs:gdt_ptr

	mov %cr0,%eax
	or $X86_CR0_PE,%al
	mov %eax,%cr0

	ljmpl $INMATE_CS32,$start32 + FSEGMENT_BASE


	.code32
start32:
	mov %cr4,%eax
	or $X86_CR4_PAE,%eax
	mov %eax,%cr4

	mov $pml4,%eax
	mov %eax,%cr3

	movl $MSR_MTRR_DEF_TYPE,%ecx
	rdmsr
	or $MTRR_ENABLE,%eax
	wrmsr

	movl $MSR_EFER,%ecx
	rdmsr
	or $EFER_LME,%eax
	wrmsr

	mov $(X86_CR0_PG | X86_CR0_WP | X86_CR0_PE),%eax
	mov %eax,%cr0

	ljmpl $INMATE_CS64,$start64 + FSEGMENT_BASE

	.code64
start64:
	xor %rbx,%rbx
	xchg ap_entry,%rbx
	or %rbx,%rbx
	jnz call_entry

	mov $1,%edi
	lock xadd %edi,cpu_number + FSEGMENT_BASE

	cmp $SMP_MAX_CPUS,%edi
	jae stop

	mov $0x01,%eax
	cpuid
	shr $24,%ebx
	mov %bl,smp_cpu_ids(%edi)

	lock incl smp_num_cpus

	cmp $0,%edi
	jne stop

	xor %rax,%rax
	mov $bss_start,%rdi
	mov $bss_qwords,%rcx
	rep stosq

	mov $inmate_main,%rbx

call_entry:
	mov $stack_top,%rsp
	callq *%rbx

stop:	cli
	hlt
	jmp stop


	.pushsection ".data"

	.globl ap_entry
ap_entry:
	.quad	0

	.globl smp_num_cpus
smp_num_cpus:
	.long	0

	.globl smp_cpu_ids
smp_cpu_ids:
	.fill	SMP_MAX_CPUS, 1, 0

	.popsection

cpu_number:
	.long	0

	.align(16)
gdt:
	.quad	0
	.quad	0x00c09b000000ffff
	.quad	0x00af9b000000ffff

gdt_ptr:
	.short	gdt_ptr - gdt - 1
	.long	gdt + FSEGMENT_BASE


	.section ".rodata"

	.align(4096)
pml4:
	.quad	pdpt + 0x003

	.align(4096)
pdpt:
	.quad	pd + 0x003

	.align(4096)
pd:
	.quad	0x0000000000000083
	.align(4096)
