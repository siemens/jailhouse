/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015-2016 Huawei Technologies Duesseldorf GmbH
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

/* We have no memory management during early init; three pages is the
 * minimum we can get away with to switch on the MMU with identity
 * mapping for they hypervisor firmware and the UART.
 *
 * TODO: find a way to avoid having these three empty pages in the
 * Jailhouse binary!
 */
#define ARCH_SECTIONS							\
	. = ALIGN(PAGE_SIZE);						\
	.bootstrap_page_tables : {					\
		bootstrap_pt_l0 = .;					\
		. = . + PAGE_SIZE;					\
		bootstrap_pt_l1 = .;					\
		. = . + PAGE_SIZE;					\
		bootstrap_pt_wildcard = .;				\
		. = . + PAGE_SIZE;					\
	}
