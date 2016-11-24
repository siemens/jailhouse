/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

void inmate_main(void)
{
	unsigned long dtb;
	void (*entry)(u64 dtb, u64 x1, u64 x2, u64 x3);

	entry = (void *)cmdline_parse_int("kernel", 0);
	dtb = cmdline_parse_int("dtb", 0);

	entry(dtb, 0, 0, 0);
}
