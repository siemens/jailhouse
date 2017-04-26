/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/config.h>

#ifndef CONFIG_INMATE_BASE
#define CONFIG_INMATE_BASE 0x0
#endif

SECTIONS {
	. = CONFIG_INMATE_BASE;
	.boot		: { *(.boot) }

	. = CONFIG_INMATE_BASE + 0x100;
	.cmdline	: {
		*(.cmdline)
		BYTE(0); /* empty string in case no buffer is provided */
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

	.bss		: {
		bss_start = .;
		*(.bss)
		. = ALIGN(4);
	}
	bss_dwords = SIZEOF(.bss) / 4;

	. = ALIGN(4096);
	. += 0x1000;
	stack_top = .;
}

ENTRY(__reset_entry)
