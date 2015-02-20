/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2015
 *
 * Authors:
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

#ifndef _JAILHOUSE_DRIVER_H
#define _JAILHOUSE_DRIVER_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define JAILHOUSE_CELL_ID_NAMELEN	31

struct jailhouse_cell_create {
	__u64 config_address;
	__u32 config_size;
	__u32 padding;
};

struct jailhouse_preload_image {
	__u64 source_address;
	__u64 size;
	__u64 target_address;
	__u64 padding;
};

struct jailhouse_cell_id {
	__s32 id;
	__u32 padding;
	char name[JAILHOUSE_CELL_ID_NAMELEN + 1];
};

struct jailhouse_cell_load {
	struct jailhouse_cell_id cell_id;
	__u32 num_preload_images;
	__u32 padding;
	struct jailhouse_preload_image image[];
};

#define JAILHOUSE_CELL_ID_UNUSED	(-1)

#define JAILHOUSE_ENABLE		_IOW(0, 0, void *)
#define JAILHOUSE_DISABLE		_IO(0, 1)
#define JAILHOUSE_CELL_CREATE		_IOW(0, 2, struct jailhouse_cell_create)
#define JAILHOUSE_CELL_LOAD		_IOW(0, 3, struct jailhouse_cell_load)
#define JAILHOUSE_CELL_START		_IOW(0, 4, struct jailhouse_cell_id)
#define JAILHOUSE_CELL_DESTROY		_IOW(0, 5, struct jailhouse_cell_id)

#endif /* !_JAILHOUSE_DRIVER_H */
