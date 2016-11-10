/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Toshiba, 2016
 *
 * Authors:
 *  Daniel Sangorrin <daniel.sangorrin@toshiba.co.jp>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/io.h>
#include <asm/vga.h>

#define VGA_WIDTH		80
#define VGA_HEIGHT		25
#define VGA_BLACK		0x0
#define VGA_BRIGHT_GREEN	0xA
#define VGA_BG_COLOR		VGA_BLACK
#define VGA_FG_COLOR		VGA_BRIGHT_GREEN
#define VGA_ATTRIBUTE		((VGA_BG_COLOR | VGA_FG_COLOR) << 8)
#define VGA_U16(c)		((u16)(c) | (u16)VGA_ATTRIBUTE)
#define ASCII_NONPRINTABLE	'?'

static u16 *vga_mem;

void vga_init(void)
{
	vga_mem = hypervisor_header.debug_console_base;
}

static void vga_scroll(void)
{
	unsigned int i;

	for (i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
		vga_mem[i] = vga_mem[i + VGA_WIDTH];

	for (i = 0; i < VGA_WIDTH; i++)
		vga_mem[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = VGA_U16(' ');
}

void vga_write(const char *msg)
{
	static u32 row_line;
	unsigned int pos;
	u16 row, line;

	/* panic_printk avoids locking 'printk_lock' due to a potential
	   deadlock in case a crash occurs while holding it. For avoiding
	   a data race between printk and panic_printk we take a local
	   snapshot of both static variables and update them on return */
	row  = (u16)((row_line & 0xFFFF0000) >> 16);
	line = (u16)(row_line & 0x0000FFFF);

	while (*msg != 0) {
		if (panic_in_progress && panic_cpu != phys_processor_id())
			return;
		if (row == VGA_WIDTH || *msg == '\n') {
			row = 0;
			if (line == (VGA_HEIGHT - 1))
				vga_scroll();
			else
				line++;
		}
		pos = line * VGA_WIDTH + row;
		switch (*msg) {
		case '\n':
			msg++;
			continue;
		case ' ' ... '~':
			vga_mem[pos] = VGA_U16(*msg);
			break;
		default:
			vga_mem[pos] = VGA_U16(ASCII_NONPRINTABLE);
		}
		row++;
		msg++;
	}

	row_line = ((u32)row << 16) | (u32)line;
}
