/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/paging.h>
#include <jailhouse/gen-defines.h>

void common(void);

void common(void)
{
	DEFINE(DCACHE_CLEAN_ASM, DCACHE_CLEAN);
	DEFINE(DCACHE_INVALIDATE_ASM, DCACHE_INVALIDATE);
	DEFINE(DCACHE_CLEAN_AND_INVALIDATE_ASM, DCACHE_CLEAN_AND_INVALIDATE);
}
