/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

SECTIONS {
	. = 0;
	.boot		: { *(.boot) }

	. = ALIGN(4096);
	. = . + 0x1000;
	stack_top = .;
	bss_start = .;
	.bss		: {
		*(.bss)
		*(COMMON)
	}

	. = ALIGN(4);
	.text		: {
		*(.text)
	}

	.rodata		: {
		*(.rodata)
	}

	.data		: {
		*(.data)
	}
}
