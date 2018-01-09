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

#include <asm/asm-defines.h>
#include <asm/paging.h>

/* VMX VM-exit handling */
	.globl vmx_vmexit
vmx_vmexit:
	push %rax
	push %rcx
	push %rdx
	push %rbx
	sub $8,%rsp /* placeholder for rsp */
	push %rbp
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15

	mov $LOCAL_CPU_BASE_ASM,%rdi
	call vcpu_handle_exit

	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rbp
	add $8,%rsp
	pop %rbx
	pop %rdx
	pop %rcx
	pop %rax

	vmresume

	jmp vmx_entry_failure
