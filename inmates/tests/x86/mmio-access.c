/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

#define EXPECT_EQUAL(a, b)	evaluate(a, b, __LINE__)

static bool all_passed = true;

static void evaluate(u64 a, u64 b, int line)
{
	bool passed = (a == b);

	printk("Test at line #%d %s\n", line, passed ? "passed" : "FAILED");
	if (!passed) {
		printk(" %llx != %llx\n", a, b);
		all_passed = false;
	}
}

void inmate_main(void)
{
	volatile u64 *comm_page_reg = (void *)(COMM_REGION_BASE + 0xff8);
	void *mmio_reg = (void *)(COMM_REGION_BASE + 0x1ff8);
	u64 pattern, reg64;

	printk("\n");

	/* --- Read Tests --- */

	pattern = 0x1122334455667788;
	mmio_write64(mmio_reg, pattern);
	EXPECT_EQUAL(*comm_page_reg, pattern);

	/* MOV_FROM_MEM (8b), 64-bit data, mod=0, reg=0, rm=3 */
	asm volatile("movq (%%rbx), %%rax"
		: "=a" (reg64) : "a" (0), "b" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern);

	/* MOV_FROM_MEM (8b), 32-bit data */
	asm volatile("movl (%%rbx), %%eax"
		: "=a" (reg64) : "a" (0), "b" (mmio_reg));
	EXPECT_EQUAL(reg64, (u32)pattern);

	/* MOVZXB (0f b6), to 64-bit, mod=0, reg=0, rm=3 */
	asm volatile("movzxb (%%rbx), %%rax"
		: "=a" (reg64) : "a" (0), "b" (mmio_reg));
	EXPECT_EQUAL(reg64, (u8)pattern);

	/* MOVZXW (0f b7) */
	asm volatile("movzxw (%%rbx), %%rax"
		: "=a" (reg64) : "a" (0), "b" (mmio_reg));
	EXPECT_EQUAL(reg64, (u16)pattern);

	/* MEM_TO_AX (a1), 64-bit data, 64-bit address */
	asm volatile("movabs (0x101ff8), %%rax"
		: "=a" (reg64) : "a" (0));
	EXPECT_EQUAL(reg64, pattern);

	/* MEM_TO_AX (a1), 32-bit data, 64-bit address */
	asm volatile("movabs (0x101ff8), %%eax"
		: "=a" (reg64) : "a" (0));
	EXPECT_EQUAL(reg64, (u32)pattern);

	printk("MMIO read test %s\n\n", all_passed ? "passed" : "FAILED");

	/* --- Write Tests --- */

	all_passed = true;
	pattern = 0x8899aabbccddeeff;
	mmio_write64(mmio_reg, ~pattern);
	EXPECT_EQUAL(*comm_page_reg, ~pattern);

	/* MOV_TO_MEM (89), 64-bit data, mod=0, reg=0, rm=4, SIB.base=5 (disp32) */
	asm volatile("movq %%rax, (0x101ff8)"
		: : "a" (pattern));
	EXPECT_EQUAL(*comm_page_reg, pattern);

	pattern = ~pattern;
	/* MOV_TO_MEM (89), 64-bit data, mod=0, reg=0, rm=5 (rip+disp32) */
	asm volatile("movq %%rax, cmdline+0x101ef8(%%rip)"
		: : "a" (pattern));
	EXPECT_EQUAL(*comm_page_reg, pattern);

	/* IMMEDIATE_TO_MEM (c7), 64-bit data, mod=0, reg=0, rm=3 */
	asm volatile("movq %0, (%%rbx)"
		: : "i" (0x12345678), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* IMMEDIATE_TO_MEM (c7), 64-bit data, mod=0, reg=0, rm=3, sign-extend */
	asm volatile("movq %0, (%%rbx)"
		: : "i" (0xccddeeff), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, 0xffffffffccddeeff);

	mmio_write64(mmio_reg, 0x1122334455667788);
	/* IMMEDIATE_TO_MEM (c7), 32-bit data */
	asm volatile("movl %0, (%%rbx)"
		: : "i" (0xccddeeff), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344ccddeeff);

	mmio_write64(mmio_reg, 0x1122334455667788);
	/* IMMEDIATE_TO_MEM (c7), 32-bit data, mod=1 (disp8), reg=0, rm=3 */
	asm volatile("movl %0, 0x10(%%rbx)"
		: : "i" (0xccddeeff), "b" (mmio_reg - 0x10));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344ccddeeff);

	/* IMMEDIATE_TO_MEM (c7), 32-bit data, mod=2 (disp32), reg=0, rm=3 */
	asm volatile("movl %0, 0x10000000(%%rbx)"
		: : "i" (0xccddeeff), "b" (mmio_reg - 0x10000000));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344ccddeeff);

	/* MOVB_TO_MEM (88), mod=0, reg=0, rm=3 */
	asm volatile("mov %%al, (%%rbx)"
		: : "a" (0x99), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344ccddee99);

	/* MOV_TO_MEM (89), 64-bit data, mod=1 (disp8), reg=0, rm=3 */
	asm volatile("movq %%rax, 0x10(%%rbx)"
		: : "a" (0x12345678), "b" (mmio_reg - 0x10));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* MOV_TO_MEM (89), 64-bit data, mod=2 (disp32), reg=0, rm=3 */
	asm volatile("movq %%rax, 0x10000000(%%rbx)"
		: : "a" (0x12345678), "b" (mmio_reg - 0x10000000));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* MOV_TO_MEM (89), 64-bit data, mod=0, reg=0, rm=4 (SIB) */
	asm volatile("movq %%rax, (%%rbx,%%rcx)"
		: : "a" (0x12345678), "b" (mmio_reg), "c" (0));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* MOV_TO_MEM (89), 64-bit data, mod=2 (disp32), reg=0, rm=4 (SIB) */
	asm volatile("movq %%rax, 0x10000000(%%rbx,%%rcx)"
		: : "a" (0x12345678), "b" (mmio_reg - 0x10000000), "c" (0));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	printk("MMIO write test %s\n", all_passed ? "passed" : "FAILED");
}
