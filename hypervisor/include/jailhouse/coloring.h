/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Universita' di Modena e Reggio Emilia, 2020
 *
 * Authors:
 *  Luca Miccio <lucmiccio@gmail.com>
 *  Marco Solieri <ms@xt3.it>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */
#ifndef _JAILHOUSE_COLORING_H
#define _JAILHOUSE_COLORING_H

#include <jailhouse/cell.h>

#ifdef CONFIG_COLORING
/**
 * Get the way size of last level cache
 */
unsigned long get_llc_way_size(void);

/**
 * Init cache coloring on the platform
 *
 * @param llc_way_size	Last level cache way size in bytes
 *
 * @return 0 on success, negative error code otherwise.
 */
int coloring_paging_init(unsigned int llc_way_size);

/**
 * Init cache coloring data for the cell
 *
 * @param cell		Cell for which the initialization shall be done.
 *
 * @return 0 on success, negative error code otherwise.
 */
int coloring_cell_init(struct cell *cell);

/**
 * Return physical page address that conforms to the colors selection
 * given in color_bitmask
 *
 * @param phys		Physical address start
 * @param color_bitmask	Mask asserting the color indices to be used
 *
 * @return The lowest physical page address being greater or equal than
 * @param phys and belonging to @param color_bitmask
 */
unsigned long next_colored(unsigned long phys, unsigned long *color_bitmask);

/**
 * Return the end address based on color selection
 *
 * @param start		Address physical start
 * @param size		Size in bytes
 * @param color_bitmask	Mask asserting the color indices to be used
 *
 * @return The address after @param size memory space starting at @param start
 * using coloring selection in @param color_bitmask.
 */
unsigned long get_end_addr(unsigned long start, unsigned long size,
	unsigned long *color_bitmask);
#else
static inline unsigned long get_llc_way_size(void)
{
	return 0;
}

static inline int coloring_paging_init(unsigned int llc_way_size)
{
	return 0;
}

static inline int coloring_cell_init(struct cell *cell)
{
	return 0;
}

static inline unsigned long
next_colored(unsigned long phys, unsigned long *color_bitmask)
{
	return phys;
}

static inline unsigned long
get_end_addr(unsigned long start, unsigned long size,
	unsigned long *color_bitmask)
{
	return (start + size);
}
#endif /* !CONFIG_COLORING */

#endif /* !_JAILHOUSE_COLORING_H */
