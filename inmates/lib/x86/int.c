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

#define X2APIC_SPIV		0x80f

#define APIC_EOI_ACK		0

extern u8 irq_entry[];

static int_handler_t __attribute__((used)) int_handler[MAX_INTERRUPT_VECTORS];

void int_init(void)
{
	write_msr(X2APIC_SPIV, 0x1ff);
}

void int_set_handler(unsigned int vector, int_handler_t handler)
{
	unsigned int int_number = vector - 32;
	u64 entry = (unsigned long)irq_entry + int_number * 16;

	int_handler[int_number] = handler;

	idt[vector * 2] = (entry & 0xffff) | (INMATE_CS64 << 16) |
		((0x8e00 | (entry & 0xffff0000)) << 32);
	idt[vector * 2 + 1] = entry >> 32;
}

asm(
".macro eoi\n\t"
	/* write 0 as ack to x2APIC EOI register (0x80b) */
	"xor %eax,%eax\n\t"
	"xor %edx,%edx\n\t"
	"mov $0x80b,%ecx\n\t"
	"wrmsr\n"
".endm\n"

".macro irq_prologue irq\n\t"
#ifdef __x86_64__
	"push %rax\n\t"
	"mov $irq * 8,%rax\n\t"
#else
	"push %eax\n\t"
	"mov $irq * 4,%eax\n\t"
#endif
	"jmp irq_common\n"
	".balign 16\n"
".endm\n\t"

	".global irq_entry\n\t"
	".balign 16\n"
"irq_entry:\n"
"irq=0\n"
".rept 32\n"
	"irq_prologue irq\n\t"
	"irq=irq+1\n\t"
".endr\n"

"irq_common:\n\t"
#ifdef __x86_64__
	"push %rcx\n\t"
	"push %rdx\n\t"
	"push %rsi\n\t"
	"push %rdi\n\t"
	"push %r8\n\t"
	"push %r9\n\t"
	"push %r10\n\t"
	"push %r11\n\t"

	"call *int_handler(%rax)\n\t"

	"eoi\n\t"

	"pop %r11\n\t"
	"pop %r10\n\t"
	"pop %r9\n\t"
	"pop %r8\n\t"
	"pop %rdi\n\t"
	"pop %rsi\n\t"
	"pop %rdx\n\t"
	"pop %rcx\n\t"
	"pop %rax\n\t"

	"iretq"
#else
	"push %ecx\n\t"
	"push %edx\n\t"
	"push %esi\n\t"
	"push %edi\n\t"

	"call *int_handler(%eax)\n\t"

	"eoi\n\t"

	"pop %edi\n\t"
	"pop %esi\n\t"
	"pop %edx\n\t"
	"pop %ecx\n\t"
	"pop %eax\n\t"

	"iret"
#endif
);

void int_send_ipi(unsigned int cpu_id, unsigned int vector)
{
	write_msr(X2APIC_ICR, ((u64)cpu_id << 32) | APIC_LVL_ASSERT | vector);
}
