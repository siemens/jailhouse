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
#include <jailhouse/coloring.h>
#include <jailhouse/printk.h>
#include <jailhouse/entry.h>
#include <jailhouse/cell.h>
#include <jailhouse/control.h>

#define for_each_cache_region(cache, config, counter)			\
	for ((cache) = jailhouse_cell_cache_regions(config), (counter) = 0;\
	     (counter) < (config)->num_cache_regions;			\
	     (cache)++, (counter)++)

/** Default color bitmask uses all available colors */
unsigned long color_bitmask_default[COLOR_BITMASK_SIZE];

/** Do care bits for coloring */
unsigned long addr_col_mask;

/** Max number of colors available on the platform */
#define COLORING_MAX_NUM ((addr_col_mask >> PAGE_SHIFT) + 1)

#define MSB_LONG_IDX(word) (word ? (BITS_PER_LONG - clz(word) - 1) : 0)
static inline unsigned long msb_color_bitmask(unsigned long *color_bitmask)
{
	unsigned long ret = 0;
	unsigned int layer = COLOR_BITMASK_SIZE - 1;

	if (!color_bitmask)
		return 0;

	while (!ret) {
		ret = MSB_LONG_IDX(color_bitmask[layer]);
		layer--;
	}

	return ret;
}

#define CTR_LINESIZE_MASK	0x7
#define CTR_SIZE_SHIFT		13
#define CTR_SIZE_MASK		0x3FFF
#define CTR_SELECT_L2		(1 << 1)
#define CTR_SELECT_L3		(1 << 2)
#define CTR_CTYPEn_MASK		0x7
#define CTR_CTYPE2_SHIFT	3
#define CTR_LLC_ON		(1 << 2)
#define CTR_LOC_SHIFT		24
#define CTR_LOC_MASK		0x7
#define CTR_LOC_NOT_IMPLEMENTED	(1 << 0)

unsigned long get_llc_way_size(void)
{
	unsigned int cache_sel;
	unsigned int cache_global_info;
	unsigned int cache_info;
	unsigned int cache_line_size;
	unsigned int cache_set_num;
	unsigned int cache_sel_tmp;

	arm_read_sysreg(CLIDR_EL1, cache_global_info);

	/* Check if at least L2 is implemented */
	if (((cache_global_info >> CTR_LOC_SHIFT) & CTR_LOC_MASK)
		== CTR_LOC_NOT_IMPLEMENTED) {
		printk("ERROR: L2 Cache not implemented\n");
		return trace_error(-ENODEV);
	}

	/* Save old value of CSSELR_EL1 */
	arm_read_sysreg(CSSELR_EL1, cache_sel_tmp);

	/* Get LLC index */
	if (((cache_global_info >> CTR_CTYPE2_SHIFT) & CTR_CTYPEn_MASK)
		== CTR_LLC_ON)
		cache_sel = CTR_SELECT_L2;
	else
		cache_sel = CTR_SELECT_L3;

	/* Select the correct LLC in CSSELR_EL1 */
	arm_write_sysreg(CSSELR_EL1, cache_sel);

	/* Ensure write */
	isb();

	/* Get info about the LLC */
	arm_read_sysreg(CCSIDR_EL1, cache_info);

	/* ARM TRM: (Log2(Number of bytes in cache line)) - 4. */
	cache_line_size = 1 << ((cache_info & CTR_LINESIZE_MASK) + 4);
	/* ARM TRM: (Number of sets in cache) - 1 */
	cache_set_num = ((cache_info >> CTR_SIZE_SHIFT) & CTR_SIZE_MASK) + 1;

	/* Restore value in CSSELR_EL1 */
	arm_write_sysreg(CSSELR_EL1, cache_sel_tmp);

	/* Ensure write */
	isb();

	return (cache_line_size * cache_set_num);
}

int coloring_paging_init(unsigned int llc_way_size)
{
	unsigned int i;

	if (!llc_way_size) {
		llc_way_size = get_llc_way_size();
		if (!llc_way_size)
			return -ENODEV;
	}

	/**
	 * Setup addr_col_mask
	 * This mask represents the bits in the address that can be used
	 * for defining available colors
	 */
	for (i = PAGE_SHIFT; i < MSB_LONG_IDX(llc_way_size); i++)
		set_bit(i, &addr_col_mask);

	if (COLORING_MAX_NUM > MAX_COLOR_SUPPORTED)
		return -ENOMEM;

	/* Setup default color bitmask */
	for (i = 0; i < COLORING_MAX_NUM; i++)
		set_bit(i, color_bitmask_default);

	printk("Coloring information:\n");
	printk("LLC way size: %uKiB\n", llc_way_size >> 10);
	printk("Address color mask: 0x%lx\n", addr_col_mask);
	printk("Max number of avail. colors: %ld\n", COLORING_MAX_NUM);
	return 0;
}

int coloring_cell_init(struct cell *cell)
{
	const struct jailhouse_cache *cache;
	int counter = 0;
	int i;

	memset(cell->arch.color_bitmask, 0,
		sizeof(unsigned long) * COLOR_BITMASK_SIZE);

	/* Root cell is currently not supported */
	if (cell == &root_cell)
		return 0;

	for_each_cache_region(cache, cell->config, counter) {
		if ((cache->start + cache->size) > COLORING_MAX_NUM ||
			!cache->size) {
			printk("Wrong color config. Max value is %ld\n",
				COLORING_MAX_NUM);
			return -ERANGE;
		}

		for (i = cache->start; i < (cache->start + cache->size); i++)
			set_bit(i, cell->arch.color_bitmask);
	}

	/* If no coloring configuration is provided, use all colors available */
	if (!counter)
		memcpy(cell->arch.color_bitmask, color_bitmask_default,
			sizeof(unsigned long) * COLOR_BITMASK_SIZE);

	printk("Cell [%s] color config: 0x%lx%lx%lx%lx\n",
		cell->config->name,
		cell->arch.color_bitmask[3], cell->arch.color_bitmask[2],
		cell->arch.color_bitmask[1], cell->arch.color_bitmask[0]);

	return 0;
}

unsigned long next_colored(unsigned long phys, unsigned long *color_bitmask)
{
	unsigned int high_idx;
	unsigned int phys_col_id;
	unsigned long retval = phys;

	if (!color_bitmask)
		return phys;

	high_idx = MSB_LONG_IDX(addr_col_mask);

	phys_col_id = (phys & addr_col_mask) >> PAGE_SHIFT;
	/**
	 * Loop over all possible colors starting from `phys_col_id` and find
	 * the next color id that belongs to `color_bitmask`.
	 */
	while (!test_bit(phys_col_id, color_bitmask)) {
		/**
		 * If we go out of bounds, restart from 0 and carry 1
		 * outside addr_col_mask MSB.
		 */
		if (phys_col_id > msb_color_bitmask(color_bitmask)) {
			phys_col_id = 0;
			retval += 1UL << (high_idx + 1);
		} else
			phys_col_id++;
	}

	/* Reset old color configuration */
	retval &= ~(addr_col_mask);
	retval |= (phys_col_id << PAGE_SHIFT);

	return retval;
}

unsigned long get_end_addr(unsigned long start, unsigned long size,
	unsigned long *color_bitmask)
{
	unsigned color_num = 0;

	/* Get number of colors from mask */
	for (int i = 0; i < MAX_COLOR_SUPPORTED; i++)
		if (test_bit(i, color_bitmask))
			color_num++;

	/* Check if start address is compliant to color selection */
	start = next_colored(start, color_bitmask);

	return start + PAGE_ALIGN((size*COLORING_MAX_NUM)/color_num);
}
