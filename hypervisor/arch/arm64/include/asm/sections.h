/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015-2016 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) 2016 Siemens AG
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

/*
 * We have no memory management during early init; four pages is the minimum we
 * can get away with to switch on the MMU with mappings for the hypervisor
 * firmware and the UART as well as identity mapping for the trampoline code
 * page.
 */
#define ARCH_SECTIONS							\
	. = ALIGN(PAGE_SIZE);						\
	.bootstrap_page_tables : {					\
		bootstrap_pt_l0 = .;					\
		. = . + PAGE_SIZE;					\
		bootstrap_pt_l1_hyp_uart = .;				\
		. = . + PAGE_SIZE;					\
		bootstrap_pt_l1_trampoline = .;				\
		. = . + PAGE_SIZE;					\
		bootstrap_pt_l2_hyp_uart = .;				\
		. = . + PAGE_SIZE;					\
	}								\
	.trampoline : {							\
		__trampoline_start = .;					\
		*(.trampoline)						\
	}
