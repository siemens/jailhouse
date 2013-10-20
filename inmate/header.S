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

#define X86_CR0_PE	0x00000001
#define X86_CR0_WP	0x00010000
#define X86_CR0_PG	0x80000000

#define X86_CR4_PAE	0x00000020

#define MSR_EFER	0xc0000080
#define EFER_LME	0x00000100

	.code16gcc
	.section ".boot", "ax"

	ljmp $0xf000,$start16


	.section ".startup", "ax"

start16:
	cs,lgdtl gdt_ptr

	mov %cr0,%eax
	or $X86_CR0_PE,%al
	mov %eax,%cr0

	ljmpl $LOADER_CS32,$start32 + FSEGMENT_BASE


	.code32
start32:
	mov %cr4,%eax
	or $X86_CR4_PAE,%eax
	mov %eax,%cr4

	mov $pml4 + FSEGMENT_BASE,%eax
	mov %eax,%cr3

	movl $MSR_EFER,%ecx
	rdmsr
	or $EFER_LME,%eax
	wrmsr

	mov $(X86_CR0_PG | X86_CR0_WP | X86_CR0_PE),%eax
	mov %eax,%cr0

	ljmpl $INMATE_CS,$start64 + FSEGMENT_BASE

	.code64
start64:
	mov $stack_top,%rsp

	mov $inmate_main,%rax
	jmpq *%rax


	.align(16)
gdt:
	.quad	0
	.quad	0x00c09b000000ffff
	.quad	0x00af9b000000ffff

gdt_ptr:
	.short	gdt_ptr - gdt - 1
	.long	gdt + FSEGMENT_BASE

	.align(4096)
pml4:
	.quad	pdpt + FSEGMENT_BASE + 0x003

	.align(4096)
pdpt:
	.quad	pd + FSEGMENT_BASE + 0x003

	.align(4096)
pd:
	.quad	0x0000000000000083


	.global irq_entry
	.balign 16
irq_entry:
	push %rax
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11

	call irq_handler - FSEGMENT_BASE

	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %rax

	iretq


/* to please linker if irq_entry remains unused */
	.weak irq_handler
irq_handler:
