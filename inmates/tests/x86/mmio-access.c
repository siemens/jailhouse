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
#include <test.h>

extern u8 __reset_entry[]; /* assumed to be at 0 */

/*
 * mmio-access tests different memory access strategies that are intercepted by
 * the hypervisor. Therefore, it maps a second page right behind the
 * comm_region. Access to 0xff8-0xfff within that page will be intercepted by
 * the hypervisor. The hypervisor will redirect the access to the comm_region.
 * By reading back those values from the comm_region, we can verify that the
 * access was successful.
 */
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

	/* MOV_FROM_MEM (8b), 16-bit data, Ox66 OP size prefix */
	asm volatile("mov (%%rax), %%ax" : "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL((u16)reg64, (u16)pattern);

	/* MOV_FROM_MEM (8b), 64-bit data, mod=0, reg=0, rm=3 */
	asm volatile("movq (%%rbx), %%rax"
		: "=a" (reg64) : "a" (0), "b" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern);

	/* MOV_FROM_MEM (8b), 32-bit data */
	asm volatile("movl (%%rbx), %%eax"
		: "=a" (reg64) : "a" (0), "b" (mmio_reg));
	EXPECT_EQUAL(reg64, (u32)pattern);

	/* MOV_FROM_MEM (8b), 32-bit data, 32-bit address */
	asm volatile("movl (%%ebx), %%eax"
		: "=a" (reg64) : "a" (0), "b" (mmio_reg));
	EXPECT_EQUAL(reg64, (u32)pattern);

	/* MOV_FROM_MEM (8a), 8-bit data */
	asm volatile("movb (%%rax), %%al"
		: "=a" (reg64) : "a" (mmio_reg));
	/* %al should contain 0x88, while high bits should still hold the rest
	 * of mmio_reg */
	EXPECT_EQUAL(reg64,
		     ((unsigned long)mmio_reg & ~0xffUL) | (pattern & 0xff));

	/* MOV_FROM_MEM (8a), 8-bit data, 0x66 OP size prefix (ignored) */
	asm volatile("data16 mov (%%rax), %%al"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64,
		     ((unsigned long)mmio_reg & ~0xffUL) | (pattern & 0xff));

	/* MOVZX test cases */

	/*
	 * First three tests: MOVZXB (0f b6) with 64-bit address, varying
	 * register width (rax, eax, ax)
	 */

	/* MOVZXB (48 0f b6), 8-bit data, 64-bit address, clear bits 8-63 */
	asm volatile("movzxb (%%rax), %%rax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern & 0xff);

	/* MOVZXB (0f b6), 8-bit data, 64-bit address, clear bits 8-63
	 * Exposes the same behaviour as 48 0f b6. */
	asm volatile("movzxb (%%rax), %%eax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern & 0xff);

	/* MOVZXB (66 0f b6), 8-bit data, clear bits 8-15, preserve 16-63,
	 * operand size prefix */
	asm volatile("movzxb (%%rax), %%ax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64,
		     ((unsigned long)mmio_reg & ~0xffffUL) | (pattern & 0xff));

	/*
	 * Second three tests: MOVZXB (0f b6) with 32-bit address, varying
	 * register width (rax, eax, ax).
	 *
	 * These pattern will cover cases, where we have, e.g., both operand
	 * prefixes (address size override prefix and operand size override
	 * prefix), and a REX + adress size override prefix.
	 */

	/* MOVZXB (67 48 0f b6), 8-bit data, clear bits 8-63, 32-bit address,
	 * REX_W, AD SZ override prefix */
	asm volatile("movzxb (%%eax), %%rax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern & 0xff);

	/* MOVZXB (67 0f b6), 8-bit data, clear bits 8-63, 32-bit address,
	 * AD SZ override prefix. Exposes the same behaviour as 67 48 0f b6. */
	asm volatile("movzxb (%%eax), %%eax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern & 0xff);

	/* MOVZXB (67 66 0f b6), 8-bit data, clear bits 8-15, preserve 16-63,
	 * 32-bit address, AD SZ override prefix, OP SZ override prefix */
	asm volatile("movzxb (%%eax), %%ax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64,
		     ((unsigned long)mmio_reg & ~0xffffUL) | (pattern & 0xff));

	/*
	 * Three tests for: MOVZXW (0f b7) with 64-bit address, varying
	 * register width (rax, eax, ax).
	 */

	/* MOVZXW (48 0f b7), 16-bit data, clear bits 16-63, 64-bit address */
	asm volatile("movzxw (%%rax), %%rax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern & 0xffff);

	/* MOVZXW (0f b7), 16-bit data, clear bits 16-63, 64-bit address.
	 * Exposes the same behaviour as 48 0f b7. */
	asm volatile("movzxw (%%rax), %%eax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern & 0xffff);

	/* MOVZXW (66 0f b7), 16-bit data, preserve bits 16-63, OP SZ prefix.
	 * Practically working, but not specified by the manual (it's
	 * effectively a 16->16 move). */
	asm volatile(".byte 0x66, 0x0f, 0xb7, 0x00"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, ((unsigned long)mmio_reg & ~0xffffUL) |
			     (pattern & 0xffff));

	/*
	 * Last but not least: MOVZXW (0f b7) with 32-bit address, varying
	 * register width (rax, eax, ax).
	 */

	/* MOVZXW (67 48 0f b7), 16-bit data, clear bits 16-63, 32-bit address,
	 * AD SZ prefix, REX_W */
	asm volatile("movzxw (%%eax), %%rax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern & 0xffff);

	/* MOVZXW (67 0f b7), 16-bit data, clear bits 16-63, 32-bit address,
	 * AD SZ prefix. Exposes same behaviour as 67 48 0f b7. */
	asm volatile("movzxw (%%eax), %%eax"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, pattern & 0xffff);

	/* MOVZXW (67 66 0f b7), 16-bit data, preserve bits 16-63, 32-bit address,
	 * AD SZ prefix, OP SZ prefix. See also 66 0f b7: not an official
	 * instruction */
	asm volatile(".byte 0x67, 0x66, 0x0f, 0xb7, 0x00"
		: "=a" (reg64) : "a" (mmio_reg));
	EXPECT_EQUAL(reg64, ((unsigned long)mmio_reg & ~0xffffUL) |
			     (pattern & 0xffff));

	/* MEM_TO_AX (a1), 64-bit data, 64-bit address */
	asm volatile("movabs (0x101ff8), %%rax"
		: "=a" (reg64) : "a" (0));
	EXPECT_EQUAL(reg64, pattern);

	/* MEM_TO_AX (a1), 32-bit data, 64-bit address */
	asm volatile("movabs (0x101ff8), %%eax"
		: "=a" (reg64) : "a" (0));
	EXPECT_EQUAL(reg64, (u32)pattern);

	reg64 = 0ULL;
	/* MEM_TO_AX (a1), 64-bit data, 32-bit address, AD SZ override prefix */
	asm volatile("addr32 mov 0x101ff8, %%rax"
		: "=a" (reg64) : "a" (0));
	EXPECT_EQUAL(reg64, pattern);

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
	asm volatile("movq %%rax, __reset_entry+0x101ff8(%%rip)"
		: : "a" (pattern));
	EXPECT_EQUAL(*comm_page_reg, pattern);

	/* MOV_TO_MEM (88), 8-bit data */
	asm volatile("movb %%al, (%%rbx)"
		: : "a" (0x42), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, (pattern & ~0xffUL) | 0x42);

	/* MOV_TO_MEM (88), 8-bit data, OP size prefix */
	asm volatile("data16 mov %%al, (%%rbx)" : : "a" (0x23), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, (pattern & ~0xffUL) | 0x23);

	/* MOV_TO_MEM (89), 16-bit data, OP size prefix */
	asm volatile("mov %%ax, (%%rbx)" : : "a" (0x2342), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, (pattern & ~0xffffUL) | 0x2342);

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
	/* IMMEDIATE_TO_MEM (c7), 32-bit data, 32-bit address */
	asm volatile("movl %0, (%%ebx)"
		: : "i" (0xccddeeff), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344ccddeeff);

	mmio_write64(mmio_reg, 0x1122334455667788);
	/* IMMEDIATE_TO_MEM (c7), 32-bit data, mod=1 (disp8), reg=0, rm=3 */
	asm volatile("movl %0, 0x10(%%rbx)"
		: : "i" (0xccddeeff), "b" (mmio_reg - 0x10));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344ccddeeff);

	mmio_write64(mmio_reg, 0x1122334455667788);
	/* IMMEDIATE_TO_MEM (c7), 32-bit data, 32-bit address */
	asm volatile("movl %0, 0x10(%%ebx)"
		: : "i" (0xccddeeff), "b" (mmio_reg - 0x10));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344ccddeeff);

	mmio_write64(mmio_reg, 0x1122334455667788);
	/* IMMEDIATE_TO_MEM (c7), 32-bit data, mod=2 (disp32), reg=0, rm=3 */
	asm volatile("movl %0, 0x10000000(%%rbx)"
		: : "i" (0xccddeeff), "b" (mmio_reg - 0x10000000));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344ccddeeff);

	mmio_write64(mmio_reg, 0x1122334455667788);
	/* IMMEDIATE_TO_MEM (c7), 32-bit data, 32-bit address */
	asm volatile("movl %0, 0x10000000(%%ebx)"
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

	mmio_write64(mmio_reg, 0x1122334455667788);
	/* MOV_TO_MEM (89), 64-bit data, 32-bit address */
	asm volatile("movq %%rax, 0x10000000(%%ebx)"
		: : "a" (0x8765432112345678), "b" (mmio_reg - 0x10000000));
	EXPECT_EQUAL(*comm_page_reg, 0x8765432112345678);

	/* MOV_TO_MEM (89), 64-bit data, mod=0, reg=0, rm=4 (SIB) */
	asm volatile("movq %%rax, (%%rbx,%%rcx)"
		: : "a" (0x12345678), "b" (mmio_reg), "c" (0));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* MOV_TO_MEM (89), 64-bit data, mod=2 (disp32), reg=0, rm=4 (SIB) */
	asm volatile("movq %%rax, 0x10000000(%%rbx,%%rcx)"
		: : "a" (0x12345678), "b" (mmio_reg - 0x10000000), "c" (0));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	pattern = 0xdeadbeef;
	/* AX_TO_MEM (a3), 32-bit data, 32-bit address */
	asm volatile(".byte 0x67, 0x48, 0xa3, 0xf8, 0x1f, 0x10, 0x00"
		: : "a" (pattern));
	EXPECT_EQUAL(mmio_read32(mmio_reg), (u32)pattern);

	printk("MMIO write test %s\n", all_passed ? "passed" : "FAILED");
}
