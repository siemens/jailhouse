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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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
