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

#include <asm/paging.h>

SECTIONS
{
	. = 0;
	.header		: { *(.header) }

	. = ALIGN(16);
	.text		: { *(.text) }

	. = ALIGN(16);
	.rodata		: { *(.rodata) }

	. = ALIGN(16);
	.data		: { *(.data) }

	.got		: {
		__got_start = .;
		*(.got*)
		__got_end = .;
	}

	. = ALIGN(16);
	.bss		: { *(.bss) }
	__hv_core_end = .;

	. = ALIGN(PAGE_SIZE);
	__page_pool = .;

	/DISCARD/ : {
		*(.eh_frame*)
	}
}
