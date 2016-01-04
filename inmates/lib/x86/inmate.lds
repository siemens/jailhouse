/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

/*
 * Layout:
 *  0x000000..        : heap (not configured here)
 *          ..0x0e0000: stack
 *  0x0e0000..0x0effff: bss
 *  0x0f0000..0x0fffef: command line, startup code, text, rodata, data
 *  0x0ffff0..0x0fffff: startup code (boot address)
 *  0x100000..0x100fff: communication region (not configured here)
 */
SECTIONS
{
	. = 0;
	.cmdline	: {
		cmdline = 0xf0000; /* adjust relocation address of variable */
		*(.cmdline)
		BYTE(0); /* empty string in case no buffer is provided */
	}

	/* 16-bit sections */
	.startup	: { *(.startup) }

	. = 0xfff0;
	.boot		: {
		*(.boot)
		. = ALIGN(16);
	}

	/* 32/64-bit sections */
	. = 0xe0000;
	stack_top = .;
	bss_start = .;
	.bss		: {
		*(.bss)
		. = ALIGN(8);
	}
	bss_dwords = SIZEOF(.bss) / 4;
	bss_qwords = SIZEOF(.bss) / 8;

	. = 0xf0000 + ADDR(.startup) + SIZEOF(.startup);
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

ENTRY(__reset_entry)
