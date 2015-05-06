/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
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

#define X86_CR4_PSE		0x00000010

#define MSR_MTRR_DEF_TYPE	0x000002ff
#define MTRR_ENABLE		0x00000800

	.code16
	.section ".boot", "ax"

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
	or $X86_CR4_PSE,%eax
	mov %eax,%cr4

	mov $loader_pdpt + FSEGMENT_BASE,%eax
	mov %eax,%cr3

	mov $(X86_CR0_PG | X86_CR0_WP | X86_CR0_PE),%eax
	mov %eax,%cr0

	movl $MSR_MTRR_DEF_TYPE,%ecx
	rdmsr
	or $MTRR_ENABLE,%eax
	wrmsr

	mov $stack_top,%esp

	mov $INMATE_DS32,%eax
	mov %eax,%ds
	mov %eax,%es
	mov %eax,%ss

	xor %eax,%eax
	mov $bss_start,%edi
	mov $bss_dwords,%ecx
	rep stosl

	mov $inmate_main,%eax
	call *%eax

stop:	cli
	hlt
	jmp stop


	.align(16)
	.global loader_gdt
loader_gdt:
	.quad	0
	.quad	0x00cf9b000000ffff
	.quad	0x00af9b000000ffff
	.quad	0x00cf93000000ffff

gdt_ptr:
	.short	gdt_ptr - loader_gdt - 1
	.long	loader_gdt + FSEGMENT_BASE

	.align(4096)
	.global loader_pdpt
loader_pdpt:
	.long	0x00000083
	.align(4096)
