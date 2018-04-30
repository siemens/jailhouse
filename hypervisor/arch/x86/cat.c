/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2015, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/unit.h>
#include <jailhouse/utils.h>
#include <asm/cat.h>

#include <jailhouse/cell-config.h>

#define CAT_ROOT_COS	0

static unsigned int cbm_max, freed_mask;
static int cos_max = -1;
static u64 orig_root_mask;

void cat_update(void)
{
	struct cell *cell = this_cell();

	write_msr(MSR_IA32_PQR_ASSOC,
		  (u64)cell->arch.cos << PQR_ASSOC_COS_SHIFT);
	write_msr(MSR_IA32_L3_MASK_0 + cell->arch.cos, cell->arch.cat_mask);
}

/* root cell has to be stopped */
static void cat_update_cell(struct cell *cell)
{
	unsigned int cpu;

	for_each_cpu(cpu, cell->cpu_set)
		if (cpu == this_cpu_id())
			cat_update();
		else
			public_per_cpu(cpu)->update_cat = true;
}

static u32 get_free_cos(void)
{
	struct cell *cell;
	u32 cos = 0;

retry:
	for_each_cell(cell)
		if (cell->arch.cos == cos) {
			cos++;
			goto retry;
		}

	return cos;
}

static bool merge_freed_mask_to_root(void)
{
	bool updated = false;
	unsigned int n;
	u64 bit;

restart:
	for (n = 0, bit = 1; n < 64; n++, bit <<= 1)
		/* unless the root mask is empty, merge only neighboring bits */
		if (freed_mask & bit && (root_cell.arch.cat_mask & (bit << 1) ||
		     root_cell.arch.cat_mask & (bit >> 1) ||
		     root_cell.arch.cat_mask == 0)) {
			root_cell.arch.cat_mask |= bit;
			freed_mask &= ~bit;
			updated = true;

			goto restart;
		}

	return updated;
}

static bool shrink_root_cell_mask(u64 cell_mask)
{
	unsigned int lo_mask_start, lo_mask_len;
	u64 lo_mask;

	if ((root_cell.arch.cat_mask & ~cell_mask) == 0) {
		/*
		 * Try to refill the root mask from the freed mask. The root
		 * mask must not become empty, so check this first.
		 */
		if (freed_mask == 0)
			return false;

		root_cell.arch.cat_mask = 0;
		merge_freed_mask_to_root();
	} else {
		/* Shrink the root cell's mask. */
		root_cell.arch.cat_mask &= ~cell_mask;

		/*
		 * Ensure that the root mask is still contiguous:
		 *
		 * Check if taking out the new cell's mask from the root mask
		 * created two halves there. Then shrink the root mask
		 * additionally by the lower half and add that part to the
		 * freed mask.
		 *
		 * Always removing the lower half simplifies this algorithm at
		 * the price of possibly choosing the smaller sub-mask. Cell
		 * configurations can avoid this by locating non-root cell
		 * masks at the beginning of the L3 cache.
		 */
		lo_mask_start = ffsl(root_cell.arch.cat_mask);
		lo_mask_len = ffzl(root_cell.arch.cat_mask >> lo_mask_start);
		lo_mask = BIT_MASK(lo_mask_start + lo_mask_len - 1,
				   lo_mask_start);

		if (root_cell.arch.cat_mask & ~lo_mask) {
			root_cell.arch.cat_mask &= ~lo_mask;
			freed_mask |= lo_mask;
		}
	}

	printk("CAT: Shrunk root cell bitmask to %08llx\n",
	       root_cell.arch.cat_mask);
	cat_update_cell(&root_cell);

	/* Drop this mask from the freed mask in case it was queued there. */
	freed_mask &= ~cell_mask;

	return true;
}

static int cat_cell_init(struct cell *cell)
{
	const struct jailhouse_cache *cache;

	cell->arch.cos = CAT_ROOT_COS;

	if (cos_max < 0)
		return 0;

	if (cell->config->num_cache_regions > 0) {
		if (cell != &root_cell) {
			cell->arch.cos = get_free_cos();
			if (cell->arch.cos > cos_max)
				return trace_error(-EBUSY);
		}

		cache = jailhouse_cell_cache_regions(cell->config);

		if (cell->config->num_cache_regions != 1 ||
		    cache->type != JAILHOUSE_CACHE_L3 ||
		    cache->size == 0 || (cache->start + cache->size) > cbm_max)
			return trace_error(-EINVAL);

		cell->arch.cat_mask =
			BIT_MASK(cache->start + cache->size - 1, cache->start);

		if (cell != &root_cell &&
		    !(cache->flags & JAILHOUSE_CACHE_ROOTSHARED) &&
		    (root_cell.arch.cat_mask & cell->arch.cat_mask) != 0)
			if (!shrink_root_cell_mask(cell->arch.cat_mask))
				return trace_error(-EINVAL);

		cat_update_cell(cell);
	} else {
		/*
		 * The root cell always occupies COS0, using the whole cache if
		 * no restriction is specified. Cells without own cache regions
		 * share these settings.
		 */
		cell->arch.cat_mask = (cell == &root_cell) ?
			BIT_MASK(cbm_max, 0) : root_cell.arch.cat_mask;
	}

	printk("CAT: Using COS %d with bitmask %08llx for cell %s\n",
	       cell->arch.cos, cell->arch.cat_mask, cell->config->name);

	return 0;
}

static void cat_cell_exit(struct cell *cell)
{
	/*
	 * Only release the mask of cells with an own partition.
	 * cos is also CAT_ROOT_COS if CAT is unsupported.
	 */
	if (cell->arch.cos == CAT_ROOT_COS)
		return;

	/*
	 * Queue bits of released mask for returning to root that were in the
	 * original root mask as well.
	 */
	freed_mask |= cell->arch.cat_mask & orig_root_mask;

	if (merge_freed_mask_to_root()) {
		printk("CAT: Extended root cell bitmask to %08llx\n",
		       root_cell.arch.cat_mask);
		cat_update_cell(&root_cell);
	}
}

static int cat_init(void)
{
	int err;

	if (cpuid_ebx(7, 0) & X86_FEATURE_CAT &&
	    cpuid_ebx(0x10, 0) & (1 << CAT_RESID_L3)) {
		cbm_max = cpuid_eax(0x10, CAT_RESID_L3) & CAT_CBM_LEN_MASK;
		cos_max = cpuid_edx(0x10, CAT_RESID_L3) & CAT_COS_MAX_MASK;
	}

	err = cat_cell_init(&root_cell);
	orig_root_mask = root_cell.arch.cat_mask;

	return err;
}

DEFINE_UNIT_SHUTDOWN_STUB(cat);
DEFINE_UNIT_MMIO_COUNT_REGIONS_STUB(cat);
DEFINE_UNIT(cat, "Cache Allocation Technology");
