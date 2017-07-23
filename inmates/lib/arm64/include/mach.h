/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) Siemens AG, 2016-2017
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
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

#ifdef CONFIG_MACH_AMD_SEATTLE
#define CON_TYPE	"PL011"
#define CON_BASE	0xe1010000

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0xe1110000)
#define GICC_V2_BASE	((void *)0xe112f000)

#elif defined(CONFIG_MACH_FOUNDATION_V8)
#define CON_TYPE	"PL011"
#define CON_BASE	0x1c090000

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0x2c001000)
#define GICC_V2_BASE	((void *)0x2c002000)

#elif defined(CONFIG_MACH_HIKEY)
#define CON_TYPE	"PL011"
#define CON_BASE	0xf7113000

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0xf6801000)
#define GICC_V2_BASE	((void *)0xf6802000)

#elif defined(CONFIG_MACH_JETSON_TX1)
#define CON_TYPE	"8250"
#define CON_BASE	0x70006000

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0x50041000)
#define GICC_V2_BASE	((void *)0x50042000)

#elif defined(CONFIG_MACH_ZYNQMP_ZCU102)
#define CON_TYPE	"XUARTPS"
#define CON_BASE	0xff010000

#define GIC_VERSION	2
#define GICD_V2_BASE	((void *)0xf9010000)
#define GICC_V2_BASE	((void *)0xf902f000)

#elif defined(CONFIG_MACH_ESPRESSOBIN)
#define CON_TYPE	"MVEBU"
#define CON_BASE	0xd0012000

#define GIC_VERSION	3
#define GICD_V3_BASE	((void *)0xd1d00000)
#define GICR_V3_BASE	((void *)0xd1d60000)	/* CPU 1 */

#endif

#ifndef TIMER_IRQ
#define TIMER_IRQ	27
#endif
