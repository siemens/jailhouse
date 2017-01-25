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
#include <asm/sections.h>

#include <asm/jailhouse_hypercall.h>

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

	ARCH_SECTIONS

	. = ALIGN(16);
	.bss		: { *(.bss) }

	/* The console section shall only contain the hypervisor console. This
	 * section and the next section must be aligned to PAGE_SIZE, as we
	 * will map the console section, and only that section, as a whole page
	 * to the root cell. */


	. = ALIGN(PAGE_SIZE);
	.console	: { *(.console) }

	. = ALIGN(PAGE_SIZE);
	__page_pool = .;

	.eh_frame	: { *(.eh_frame*) }
}
