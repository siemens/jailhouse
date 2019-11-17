/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/processor.h>
#include <jailhouse/string.h>
#include <asm/efifb.h>

#define EFIFB_MAX_WIDTH		1920
#define EFIFB_MAX_HEIGHT	1080
#define EFIFB_FONT_WIDTH	8
#define EFIFB_FONT_HEIGHT	16
#define EFIFB_LINE_LEN		(EFIFB_FONT_WIDTH * efifb_width)
#define EFIFB_FG_COLOR		0x00ff00	/* green */
#define EFIFB_BG_COLOR		0x000000	/* black */

asm(
"	.pushsection \".rodata\"\n"
"font8x16:\n"
"	.incbin \"altc-8x16\"\n"
"	.popsection\n"
);

extern const u8 font8x16[];

static unsigned int efifb_width, efifb_height;
static char efifb_buffer[EFIFB_MAX_HEIGHT][EFIFB_MAX_WIDTH];
static u32 row_line;

static void efifb_write_char(unsigned int line, unsigned int row, char c)
{
	unsigned int x, y, font_line, color, offs;

	for (y = 0; y < EFIFB_FONT_HEIGHT; y++) {
		font_line = font8x16[c * EFIFB_FONT_HEIGHT + y];

		for (x = 0; x < EFIFB_FONT_WIDTH; x++) {
			if (font_line & (1 << (EFIFB_FONT_WIDTH - x)))
				color = EFIFB_FG_COLOR;
			else
				color = EFIFB_BG_COLOR;
			offs = (row * EFIFB_FONT_WIDTH) + x +
				(line * EFIFB_FONT_HEIGHT + y) * EFIFB_LINE_LEN;
			((u32 *)hypervisor_header.debug_console_base)[offs] =
				color;
		}
	}

	efifb_buffer[line][row] = c;
}

static void efifb_scroll(void)
{
	unsigned int row, line;

	for (line = 0; line < (efifb_height - 1); line++)
		for (row = 0; row < efifb_width; row++)
			efifb_write_char(line, row,
					 efifb_buffer[line + 1][row]);

	for (row = 0; row < efifb_width; row++)
		efifb_write_char(efifb_height - 1, row, ' ');
}

void efifb_write(const char *msg)
{
	/*
	 * panic_printk() avoids locking 'printk_lock' due to a potential
	 * deadlock in case a crash occurs while holding it. For avoiding
	 * a data race between printk and panic_printk, we take a local
	 * snapshot of both static variables and update them on return.
	 */
	u16 row  = (u16)(row_line >> 16);
	u16 line = (u16)row_line;

	while (*msg != 0) {
		if (panic_in_progress && panic_cpu != phys_processor_id())
			return;

		if (row == efifb_width || *msg == '\n') {
			row = 0;
			if (line == efifb_height - 1)
				efifb_scroll();
			else
				line++;
		}

		if (*msg != '\n' && *msg != '\r') {
			efifb_write_char(line, row, *msg);
			row++;
		}
		msg++;
	}

	row_line = ((u32)row << 16) | (u32)line;
}

void efifb_init(void)
{
	if (FB_IS_1920x1080(system_config->debug_console.flags)) {
		efifb_width  = 1920 / EFIFB_FONT_WIDTH;
		efifb_height = 1080 / EFIFB_FONT_HEIGHT;
	} else {
		efifb_width  = 1024 / EFIFB_FONT_WIDTH;
		efifb_height =  768 / EFIFB_FONT_HEIGHT;
	}
}
