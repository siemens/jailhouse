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

SECTIONS
{
	/* 16-bit sections */
	. = 0;
	.startup	: { *(.startup) }

	. = 0xfff0;
	.boot		: {
		*(.boot)
		. = ALIGN(16);
	}

	/* 32-bit sections */
	. = 0xe0000;
	stack_top = .;
	.bss		: { *(.bss) }

	. = 0xf0000 + SIZEOF(.startup);
	.text		: AT (ADDR(.text) & 0xffff) {
		*(.text)
	}

	. = ALIGN(16);
	.rodata		: AT (ADDR(.rodata) & 0xffff) {
		*(.rodata)
	}

	. = ALIGN(16);
	.data		: AT (ADDR(.data) & 0xffff) {
		*(.data)
	}

	/DISCARD/ : {
		*(.eh_frame*)
	}
}
