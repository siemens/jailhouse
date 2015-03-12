/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define __MODEL_GFP_USER	(0x10u | 0x40u | 0x80u | 0x20000u)
#define __MODEL_GFP_NOWARN	0x200u

void *kmalloc(size_t size, unsigned flags)
{
	void *ptr;

	if (flags == (__MODEL_GFP_USER | __MODEL_GFP_NOWARN))
		__coverity_tainted_data_sanitize__(size);
	else
		__coverity_tainted_data_sink__(size);

	ptr = __coverity_alloc__(size);

	__coverity_mark_as_uninitialized_buffer__(ptr);
	__coverity_mark_as_afm_allocated__(ptr, "kfree");

	return ptr;
}
