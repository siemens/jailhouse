/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

SECTIONS {
	. = 0;
	.boot		: { *(.boot) }

	. = 0x1000;
	.cmdline	: {
		*(.cmdline)
		BYTE(0); /* empty string in case no buffer is provided */
	}

	bss_start = .;
	.bss		: {
		*(.bss)
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

	. = ALIGN(4096);
	. = . + 0x1000;
	stack_top = .;
}

ENTRY(__reset_entry)
