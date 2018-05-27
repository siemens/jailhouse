/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2018
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
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

#define HUGE_PAGE_SIZE		(2 * 1024 * 1024ULL)
#define HUGE_PAGE_MASK		(~(HUGE_PAGE_SIZE - 1))

#define ICC_IAR1_EL1		SYSREG_32(0, c12, c12, 0)
#define ICC_EOIR1_EL1		SYSREG_32(0, c12, c12, 1)
#define ICC_PMR_EL1		SYSREG_32(0, c4, c6, 0)
#define ICC_CTLR_EL1		SYSREG_32(0, c12, c12, 4)
#define ICC_IGRPEN1_EL1		SYSREG_32(0, c12, c12, 7)

#define ICC_IGRPEN1_EN		0x1

#define MAIR_ATTR_SHIFT(__n)	((__n) << 3)
#define MAIR_ATTR(__n, __attr)	((__attr) << MAIR_ATTR_SHIFT(__n))
#define MAIR_ATTR_WBRWA		0xff
#define MAIR_ATTR_DEVICE	0x00    /* nGnRnE */

/* Common definitions for page table structure in long descriptor format */
#define LONG_DESC_BLOCK 0x1
#define LONG_DESC_TABLE 0x3

#define LATTR_CONT		(1 << 12)
#define LATTR_AF		(1 << 10)
#define LATTR_INNER_SHAREABLE	(3 << 8)
#define LATTR_MAIR(n)		(((n) & 0x3) << 2)

#define LATTR_AP(n)		(((n) & 0x3) << 6)
#define LATTR_AP_RW_EL1		LATTR_AP(0x0)

#define PGD_INDEX(addr)		((addr) >> 30)
#define PMD_INDEX(addr)		(((addr) >> 21) & 0x1ff)
