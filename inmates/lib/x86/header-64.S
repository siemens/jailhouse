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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inmate.h>
#include <asm/regs.h>

	.code32
	.section ".boot", "ax"
	.globl start32
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

	ljmpl $INMATE_CS64,$start64

	.code64
start64:
	xor %rbx,%rbx
	xchg ap_entry,%rbx
	or %rbx,%rbx
	jnz call_entry

	mov $1,%edi
	lock xadd %edi,cpu_number

	cmp $SMP_MAX_CPUS,%edi
	jae stop

	mov $X86_CPUID_FEATURES, %eax
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

	mov $c_entry,%rbx

call_entry:
	xor %rsp, %rsp
	xchg stack, %rsp

	call arch_init_features

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

	.globl gdt_ptr
gdt_ptr:
	.short	gdt_ptr - gdt - 1
	.long	gdt


	.section ".rodata"

	.align(4096)
pml4:
	.quad	pdpt + PAGE_DEFAULT_FLAGS

	.align(4096)
pdpt:
	.quad	pd + PAGE_DEFAULT_FLAGS

	.align(4096)
pd:
	/* ID map 2M@0x0 */
	.quad	0x0 + (PAGE_FLAG_PS | PAGE_DEFAULT_FLAGS)
	.align(4096)
