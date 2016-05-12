/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

/* Example memory map:
 *     0x00000000 - 0x00003fff (16K) this binary
 *     0x00280000                    Image
 *     0x0fe00000                    dtb
 */

#define CMDLINE_BUFFER_SIZE	256
CMDLINE_BUFFER(CMDLINE_BUFFER_SIZE);

struct arm64_linux_header {
	u32 code0;		/* Executable code */
	u32 code1;		/* Executable code */
	u64 text_offset;	/* Image load offset, little endian */
	u64 image_size;		/* Effective Image size, little endian */
	u64 flags;		/* kernel flags, little endian */
	u64 res2;		/* = 0, reserved */
	u64 res3;		/* = 0, reserved */
	u64 res4;		/* = 0, reserved */
	u32 magic;		/* 0x644d5241 Magic number, little endian,
				   "ARM\x64" */
	u32 res5;		/* reserved (used for PE COFF offset) */
};

void inmate_main(void)
{
	struct arm64_linux_header *kernel;
	unsigned long dtb;
	void (*entry)(unsigned long);

	printk("\nJailhouse ARM64 Linux bootloader\n");

	kernel = (void *) cmdline_parse_int("kernel", 0);
	dtb = cmdline_parse_int("dtb", 0);

	if (!kernel || !dtb) {
		printk("ERROR: command line parameters kernel and dtb"
							" are required\n");
		while(1);
	}

	entry = (void*)(unsigned long) kernel;

	printk("DTB:        0x%016lx\n", dtb);
	printk("Image:      0x%016lx\n", kernel);
	printk("Image size: %lu Bytes\n", kernel->image_size);
	printk("entry:      0x%016lx\n", entry);
	if (kernel->magic != 0x644d5241)
		printk("WARNING: wrong Linux Image header magic: 0x%08x\n",
		       kernel->magic);

	entry(dtb);
}
