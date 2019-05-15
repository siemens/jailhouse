/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2019
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

struct stack_frame {
	unsigned long bp, si, dx, bx, ax;
#ifdef __x86_64__
	unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
	unsigned long cx, di;
#else
	unsigned long di, cx;
#endif
	unsigned long error_code, ip, cs, flags;
#ifdef __x86_64__
	unsigned long sp, ss;
#endif
};

extern u8 excp_entry[];

void excp_reporting_init(void)
{
	unsigned int vector;
	u64 entry;

	for (vector = 0; vector < 21; vector++) {
		entry = (unsigned long)excp_entry + vector * 16;

		idt[vector * 2] = (entry & 0xffff) | (INMATE_CS64 << 16) |
			((0x8e00 | (entry & 0xffff0000)) << 32);
		idt[vector * 2 + 1] = entry >> 32;
	}
}

static void __attribute__((used))
exception_handler(unsigned int vector, struct stack_frame *frame)
{
	/*
	 * Set the state first, in case enter an endless loop while reporting.
	 */
	comm_region->cell_state = JAILHOUSE_CELL_FAILED;

	printk("--- EXCEPTION %d ---\n", vector);
	if (vector >= 10 && vector <= 14)
		printk(" Error code: 0x%08lx\n", frame->error_code);
	printk(" CS:  0x%04lx IP: %p flags: 0x%08lx\n",
	       frame->cs, (void *)frame->ip, frame->flags);
	printk(" SP:  %p BP:  %p\n", frame + 1, (void *)frame->bp);
	printk(" AX:  %p BX:  %p CX:  %p\n",
	       (void *)frame->ax, (void *)frame->bx, (void *)frame->bx);
	printk(" DX:  %p SI:  %p DI:  %p\n",
	       (void *)frame->dx, (void *)frame->si, (void *)frame->di);
#ifdef __x86_64__
	printk(" R8:  %p R9:  %p R10: %p\n",
	       (void *)frame->r8, (void *)frame->r9, (void *)frame->r10);
	printk(" R11: %p R12: %p R13: %p\n",
	       (void *)frame->r11, (void *)frame->r12, (void *)frame->r13);
	printk(" R14: %p R15: %p\n",
	       (void *)frame->r14, (void *)frame->r15);
#endif

	stop();
}

asm(
".macro excp_prologue vector\n\t"
	"push $0\n\t"
	"excp_error_prologue vector\n\t"
".endm\n"

".macro excp_error_prologue vector\n\t"
#ifdef __x86_64__
	"push %rdi\n\t"
	"mov $vector,%rdi\n\t"
#else
	"push %ecx\n\t"
	"mov $vector,%ecx\n\t"
#endif
	"jmp excp_common\n"
	".balign 16\n\t"
".endm\n\t"

	".global excp_entry\n\t"
	".balign 16\n"
"excp_entry:\n"
"vector=0\n"
".rept 8\n"
	"excp_prologue vector\n\t"
	"vector=vector+1\n\t"
".endr\n"
	"excp_error_prologue 8 \n\t"
	"excp_prologue 9\n\t"
"vector=10\n"
".rept 5\n"
	"excp_error_prologue vector\n\t"
	"vector=vector+1\n\t"
".endr\n"
	"excp_prologue 15\n\t"
	"excp_prologue 16\n\t"
	"excp_error_prologue 17\n\t"
	"excp_prologue 18\n\t"
	"excp_prologue 19\n\t"
	"excp_prologue 20\n\t"

"excp_common:\n\t"
#ifdef __x86_64__
	"push %rcx\n\t"
	"push %r15\n\t"
	"push %r14\n\t"
	"push %r13\n\t"
	"push %r12\n\t"
	"push %r11\n\t"
	"push %r10\n\t"
	"push %r9\n\t"
	"push %r8\n\t"
	"push %rax\n\t"
	"push %rbx\n\t"
	"push %rdx\n\t"
	"push %rsi\n\t"
	"push %rbp\n\t"
	"mov %rsp,%rsi\n\t"
#else
	"push %edi\n\t"
	"push %eax\n\t"
	"push %ebx\n\t"
	"push %edx\n\t"
	"push %esi\n\t"
	"push %ebp\n\t"
	"mov %esp,%edx\n\t"
#endif

	"jmp exception_handler\n\t"
);
