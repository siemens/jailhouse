/*
 * Jailhouse ARM support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Dmitry Voytik <dmitry.voytik@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

void inmate_main(void)
{
	void register (*entry)(unsigned long, unsigned long, unsigned long);
	unsigned long register dtb;

	entry = (void *)(unsigned long)cmdline_parse_int("kernel", 0);
	dtb = cmdline_parse_int("dtb", 0);

	entry(0, -1, dtb);
}
