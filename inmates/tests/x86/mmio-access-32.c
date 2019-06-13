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

void inmate_main(void)
{
	volatile u32 *comm_page_reg = (void *)(COMM_REGION_BASE + 0xff8);
	void *mmio_reg = (void *)(COMM_REGION_BASE + 0x1ff8);
	u32 pattern, reg32;

	printk("\n");

	/* --- Read Tests --- */

	pattern = 0x11223344;
	mmio_write32(mmio_reg, pattern);
	EXPECT_EQUAL(*comm_page_reg, pattern);

	/* MOV_FROM_MEM (8b), 16-bit data, 32-bit address, OP size prefix */
	asm volatile("mov (%%eax), %%ax" : "=a" (reg32) : "a" (mmio_reg));
	EXPECT_EQUAL((u16)reg32, (u16)pattern);

	/* MOV_FROM_MEM (8b), 32-bit data, 32-bit address */
	asm volatile("movl (%%ebx), %%eax"
		: "=a" (reg32) : "a" (0), "b" (mmio_reg));
	EXPECT_EQUAL(reg32, pattern);

	/* MOV_FROM_MEM (8a), 8-bit data */
	asm volatile("movb (%%eax), %%al"
		: "=a" (reg32) : "a" (mmio_reg));
	/* %al should contain 0x44, while higher bits still hold the rest of
	 * mmio_reg. Test this. */
	EXPECT_EQUAL(reg32,
		     ((unsigned long)mmio_reg & ~0xffUL) | (pattern & 0xff));

	/* MOV_FROM_MEM (8a), 8-bit data, 32-bit address, OP size prefix */
	asm volatile("data16 mov (%%eax), %%al"
		: "=a" (reg32) : "a" (mmio_reg));
	EXPECT_EQUAL(reg32,
		     ((unsigned long)mmio_reg & ~0xffUL) | (pattern & 0xff));

	/* MOVZXB (0f b6), 8-bit data, 32-bit address, zero extend bits 8-31 */
	asm volatile("movzxb (%%eax), %%eax"
		: "=a" (reg32) : "a" (mmio_reg));
	EXPECT_EQUAL(reg32, pattern & 0xff);

	/* MOVZXB (66 0f b6), 8-bit data, 32-bit address, zero extend bits 8-15,
	 * operand size prefix */
	asm volatile("movzxb (%%eax), %%ax"
		: "=a" (reg32) : "a" (mmio_reg));
	EXPECT_EQUAL(reg32,
		     ((unsigned long)mmio_reg & ~0xffff) | (pattern & 0xff));

	/* MOVZXW (0f b7), 16-bit data, 32-bit address, zero extend bits
	 * 16-31 */
	asm volatile("movzxw (%%eax), %%eax"
		: "=a" (reg32) : "a" (mmio_reg));
	EXPECT_EQUAL(reg32, pattern & 0xffff);

	/* MOVZXW (66 0f b7), 16-bit data, 32-bit address, preserve bits 16-31.
	 * Practically working, but not specified by the manual (it's
	 * effectively a 16->16 move). */
	asm volatile(".byte 0x66, 0x0f, 0xb7, 0x00"
		: "=a" (reg32) : "a" (mmio_reg));
	EXPECT_EQUAL(reg32, ((unsigned long)mmio_reg & ~0xffff) |
			     (pattern & 0xffff));

	/* MEM_TO_AX (a1), 32-bit data, 32-bit address */
	asm volatile("mov (0x101ff8), %%eax"
		: "=a" (reg32) : "a" (0));
	EXPECT_EQUAL(reg32, pattern);

	printk("MMIO read test %s\n\n", all_passed ? "passed" : "FAILED");

	/* --- Write Tests --- */

	all_passed = true;
	pattern = 0x8899aabb;
	mmio_write32(mmio_reg, ~pattern);
	EXPECT_EQUAL(*comm_page_reg, ~pattern);

	/* MOV_TO_MEM (89), 32-bit data, mod=0, reg=0, rm=4, SIB.base=5 (disp32) */
	asm volatile("movl %%eax, (0x101ff8)"
		: : "a" (pattern));
	EXPECT_EQUAL(*comm_page_reg, pattern);

	/* MOV_TO_MEM (88), 8-bit data */
	asm volatile("movb %%al, (%%ebx)"
		: : "a" (0x42), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, (pattern & ~0xffUL) | 0x42);

	/* MOV_TO_MEM (88), 8-bit data, OP size prefix */
	asm volatile("data16 mov %%al, (%%ebx)" : : "a" (0x23), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, (pattern & ~0xffUL) | 0x23);

	/* MOV_TO_MEM (89), 16-bit data, OP size prefix */
	asm volatile("mov %%ax, (%%ebx)" : : "a" (0x2342), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, (pattern & ~0xffffUL) | 0x2342);

	/* IMMEDIATE_TO_MEM (c7), 32-bit data, mod=0, reg=0, rm=3 */
	asm volatile("movl %0, (%%ebx)"
		: : "i" (0x12345678), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* IMMEDIATE_TO_MEM (c7), 32-bit data, mod=1 (disp8), reg=0, rm=3 */
	asm volatile("movl %0, 0x10(%%ebx)"
		: : "i" (0x11223344), "b" (mmio_reg - 0x10));
	EXPECT_EQUAL(*comm_page_reg, 0x11223344);

	/* IMMEDIATE_TO_MEM (c7), 32-bit data, mod=2 (disp32), reg=0, rm=3 */
	asm volatile("movl %0, 0x10000000(%%ebx)"
		: : "i" (0xccddeeff), "b" (mmio_reg - 0x10000000));
	EXPECT_EQUAL(*comm_page_reg, 0xccddeeff);

	/* MOVB_TO_MEM (88), mod=0, reg=0, rm=3 */
	asm volatile("mov %%al, (%%ebx)"
		: : "a" (0x99), "b" (mmio_reg));
	EXPECT_EQUAL(*comm_page_reg, 0xccddee99);

	/* MOV_TO_MEM (89), 32-bit data, mod=1 (disp8), reg=0, rm=3 */
	asm volatile("movl %%eax, 0x10(%%ebx)"
		: : "a" (0x12345678), "b" (mmio_reg - 0x10));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* MOV_TO_MEM (89), 32-bit data, mod=2 (disp32), reg=0, rm=3 */
	asm volatile("movl %%eax, 0x10000000(%%ebx)"
		: : "a" (0x12345678), "b" (mmio_reg - 0x10000000));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* MOV_TO_MEM (89), 32-bit data, 32-bit address */
	asm volatile("movl %%eax, 0x10000000(%%ebx)"
		: : "a" (0x87654321), "b" (mmio_reg - 0x10000000));
	EXPECT_EQUAL(*comm_page_reg, 0x87654321);

	/* MOV_TO_MEM (89), 32-bit data, mod=0, reg=0, rm=4 (SIB) */
	asm volatile("movl %%eax, (%%ebx,%%ecx)"
		: : "a" (0x12345678), "b" (mmio_reg), "c" (0));
	EXPECT_EQUAL(*comm_page_reg, 0x12345678);

	/* MOV_TO_MEM (89), 32-bit data, mod=2 (disp32), reg=0, rm=4 (SIB) */
	asm volatile("movl %%eax, 0x10000000(%%ebx,%%ecx)"
		: : "a" (0x87654321), "b" (mmio_reg - 0x10000000), "c" (0));
	EXPECT_EQUAL(*comm_page_reg, 0x87654321);

	printk("MMIO write test %s\n", all_passed ? "passed" : "FAILED");
}
