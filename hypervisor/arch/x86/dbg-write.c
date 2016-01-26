/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 * Copyright (c) Toshiba, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Daniel Sangorrin <daniel.sangorrin@toshiba.co.jp>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/entry.h>
#include <jailhouse/printk.h>
#include <jailhouse/processor.h>
#include <asm/io.h>

#define UART_TX			0x0
#define UART_DLL		0x0
#define UART_DLM		0x1
#define UART_LCR		0x3
#define UART_LCR_8N1		0x03
#define UART_LCR_DLAB		0x80
#define UART_LSR		0x5
#define UART_LSR_THRE		0x20

static u16 uart_base;

static void uart_init(void)
{
	if (system_config->debug_console.phys_start == 0 ||
	    system_config->debug_console.flags & JAILHOUSE_MEM_IO)
		return;

	uart_base = system_config->debug_console.phys_start;

	outb(UART_LCR_DLAB, uart_base + UART_LCR);
#ifdef CONFIG_UART_OXPCIE952
	outb(0x22, uart_base + UART_DLL);
#else
	outb(1, uart_base + UART_DLL);
#endif
	outb(0, uart_base + UART_DLM);
	outb(UART_LCR_8N1, uart_base + UART_LCR);
}

static void uart_write(const char *msg)
{
	char c = 0;

	while (1) {
		if (c == '\n')
			c = '\r';
		else
			c = *msg++;
		if (!c)
			break;
		while (!(inb(uart_base + UART_LSR) & UART_LSR_THRE))
			cpu_relax();
		if (panic_in_progress && panic_cpu != phys_processor_id())
			break;
		outb(c, uart_base + UART_TX);
	}
}

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

static void vga_init(void)
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

static void vga_write(const char *msg)
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

void arch_dbg_write_init(void)
{
	vga_init();
	uart_init();
}

void arch_dbg_write(const char *msg)
{
	if (vga_mem)
		vga_write(msg);
	else if (uart_base)
		uart_write(msg);
}
