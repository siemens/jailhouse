/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/cell.h>

struct unit {
	const char *name;

	int (*init)(void);
	void (*shutdown)(void);

	unsigned int (*mmio_count_regions)(struct cell *);
	int (*cell_init)(struct cell *);
	void (*cell_exit)(struct cell *);
};

#define DEFINE_UNIT(__name, __description)				\
	static const struct unit unit_##__name				\
	__attribute__((section(".units"), aligned(8), used)) = {	\
		.name			= __description,		\
		.init			= __name##_init,		\
		.shutdown		= __name##_shutdown,		\
		.mmio_count_regions	= __name##_mmio_count_regions,	\
		.cell_init		= __name##_cell_init,		\
		.cell_exit		= __name##_cell_exit,		\
	}

#define DEFINE_UNIT_SHUTDOWN_STUB(__name)				\
	static void __name##_shutdown(void) { }

#define DEFINE_UNIT_MMIO_COUNT_REGIONS_STUB(__name)			\
	static unsigned int __name##_mmio_count_regions(struct cell *cell) \
	{ return 0; }

extern struct unit __unit_array_start[0], __unit_array_end[0];

#define for_each_unit(unit)						\
	for ((unit) = __unit_array_start; (unit) < __unit_array_end; (unit)++)

#define for_each_unit_before_reverse(unit, before_unit)			\
	for ((unit) = before_unit - 1; (unit) >= __unit_array_start;	\
	     (unit)--)

#define for_each_unit_reverse(unit)					\
	for_each_unit_before_reverse(unit, __unit_array_end)
