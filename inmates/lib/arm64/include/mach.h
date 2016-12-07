/*
 * Jailhouse AArch64 support
 *
 * Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
 * Copyright (c) Siemens AG, 2016
 * 
 *
 * Authors:
 *  Antonios Motakis <antonios.motakis@huawei.com>
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifdef MACH_AMD_SEATTLE
#define CON_TYPE	"PL011"
#define CON_BASE	0xe1010000

#define GICD_V2_BASE	((void *)0xe1110000)
#define GICC_V2_BASE	((void *)0xe112f000)

#define TIMER_IRQ	27

#elif defined(MACH_FOUNDATION_V8)
#define CON_TYPE	"PL011"
#define CON_BASE	0x1c090000

#define GICD_V2_BASE	((void *)0x2c001000)
#define GICC_V2_BASE	((void *)0x2c002000)

#define TIMER_IRQ	27

#elif defined(MACH_HI6220)
#define CON_TYPE	"PL011"
#define CON_BASE	0xf7113000

#define GICD_V2_BASE	((void *)0xf6801000)
#define GICC_V2_BASE	((void *)0xf6802000)

#define TIMER_IRQ	27
#endif
