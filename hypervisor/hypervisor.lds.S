/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/paging.h>
#include <asm/sections.h>

#include <asm/jailhouse_header.h>

SECTIONS
{
	. = JAILHOUSE_BASE;
	.header		: { *(.header) }

	. = ALIGN(16);
	__text_start = .;
	.text		: { *(.text) }

	. = ALIGN(16);
	.rodata		: { *(.rodata) }

	. = ALIGN(16);
	.data		: { *(.data) }

	. = ALIGN(8);
	__init_array_start = .;
	.init_array	: { *(SORT(.init_array.*)) *(.init_array) }
	__init_array_end = .;

	ARCH_SECTIONS

	/* The console section shall only contain the hypervisor console. This
	 * section and the next section must be aligned to PAGE_SIZE, as we
	 * will map the console section, and only that section, as a whole page
	 * to the root cell. */

	. = ALIGN(PAGE_SIZE);
	.console	: { *(.console) }

	. = ALIGN(PAGE_SIZE);
	.bss		: { *(.bss) }

	. = ALIGN(PAGE_SIZE);
	__page_pool = .;

	.eh_frame	: { *(.eh_frame*) }
}
