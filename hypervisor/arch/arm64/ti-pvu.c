/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2018 Texas Instruments Incorporated - http://www.ti.com/
 *
 * TI PVU IOMMU unit
 *
 * Peripheral Virtualization Unit(PVU) is an IOMMU (memory management
 * unit for DMA) which is designed for 2nd stage address translation in a
 * real time manner.
 *
 * Unlike ARM-SMMU, all the memory mapping information is stored in the
 * local registers instead of the in-memory page tables.
 *
 * There are limitations on the number of available contexts, page sizes,
 * number of pages that can be mapped, etc.
 *
 * PVU is designed to be programmed with all the memory mapping at once.
 * Therefore, it defers the actual register programming till config_commit.
 * Also, it does not support unmapping of the pages at runtime.
 *
 * Authors:
 *  Nikhil Devshatwar <nikhil.nd@ti.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/unit.h>
#include <asm/iommu.h>
#include <asm/ti-pvu.h>

#define MAX_PVU_ENTRIES		(PAGE_SIZE / sizeof (struct pvu_tlb_entry))
#define MAX_VIRTID		7

static struct pvu_dev pvu_units[JAILHOUSE_MAX_IOMMU_UNITS];
static unsigned int pvu_count;

static const u64 pvu_page_size_bytes[] = {
	4 * 1024,
	16 * 1024,
	64 * 1024,
	2 * 1024 * 1024,
	32 * 1024 * 1024,
	512 * 1024 * 1024,
	1 * 1024 * 1024 * 1024,
	16ULL * 1024 * 1024 * 1024,
};

static inline u32 is_aligned(u64 addr, u64 size)
{
	return (addr % size) == 0;
}

static void pvu_tlb_enable(struct pvu_dev *dev, u16 tlbnum)
{
	struct pvu_hw_tlb *tlb;

	tlb = (struct pvu_hw_tlb *)dev->tlb_base + tlbnum;
	mmio_write32_field(&tlb->chain, PVU_TLB_LOG_DIS_MASK, 0);
	mmio_write32_field(&tlb->chain, PVU_TLB_EN_MASK, 1);
}

static void pvu_tlb_disable(struct pvu_dev *dev, u16 tlbnum)
{
	struct pvu_hw_tlb *tlb;

	tlb = (struct pvu_hw_tlb *)dev->tlb_base + tlbnum;
	mmio_write32_field(&tlb->chain, PVU_TLB_EN_MASK, 0);
	mmio_write32_field(&tlb->chain, PVU_TLB_LOG_DIS_MASK, 1);
}

static u32 pvu_tlb_is_enabled(struct pvu_dev *dev, u16 tlbnum)
{
	struct pvu_hw_tlb *tlb;

	tlb = (struct pvu_hw_tlb *)dev->tlb_base + tlbnum;
	if (mmio_read32_field(&tlb->chain, PVU_TLB_EN_MASK))
		return 1;
	else
		return 0;
}

static int pvu_tlb_chain(struct pvu_dev *dev, u16 tlbnum, u16 tlb_next)
{
	struct pvu_hw_tlb *tlb;

	if (tlb_next <= tlbnum || tlb_next <= dev->max_virtid)
		return -EINVAL;

	tlb = (struct pvu_hw_tlb *)dev->tlb_base + tlbnum;
	mmio_write32_field(&tlb->chain, PVU_TLB_CHAIN_MASK, tlb_next);
	return 0;
}

static u32 pvu_tlb_next(struct pvu_dev *dev, u16 tlbnum)
{
	struct pvu_hw_tlb *tlb;

	tlb = (struct pvu_hw_tlb *)dev->tlb_base + tlbnum;
	return mmio_read32_field(&tlb->chain, PVU_TLB_CHAIN_MASK);
}

static u32 pvu_tlb_alloc(struct pvu_dev *dev, u16 virtid)
{
	unsigned int i;

	for (i = dev->max_virtid + 1; i < dev->num_tlbs; i++) {
		if (dev->tlb_data[i] == 0) {
			dev->tlb_data[i] = virtid << dev->num_entries;
			dev->free_tlb_count--;
			return i;
		}
	}

	/*
	 * We should never reach here, tlb_allocation should not fail.
	 * pvu_iommu_map_memory ensures that there are enough free TLBs
	 */

	BUG();
	return 0;
}

static void pvu_tlb_flush(struct pvu_dev *dev, u16 tlbnum)
{
	struct pvu_hw_tlb_entry *entry;
	struct pvu_hw_tlb *tlb;
	u32 i;

	pvu_tlb_disable(dev, tlbnum);
	tlb = (struct pvu_hw_tlb *)dev->tlb_base + tlbnum;

	for (i = 0; i < dev->num_entries; i++) {

		entry = &tlb->entry[i];
		mmio_write32(&entry->reg0, 0x0);
		mmio_write32(&entry->reg1, 0x0);
		mmio_write32(&entry->reg2, 0x0);
		mmio_write32(&entry->reg4, 0x0);
		mmio_write32(&entry->reg5, 0x0);
		mmio_write32(&entry->reg6, 0x0);
	}

	mmio_write32(&tlb->chain, 0x0);

	if (i < dev->max_virtid) {
		dev->tlb_data[tlbnum] = 0x0 | i << dev->num_entries;
	} else {
		/* This was a chained TLB */
		dev->tlb_data[tlbnum] = 0x0;
		dev->free_tlb_count++;
	}

}

static void pvu_entry_enable(struct pvu_dev *dev, u16 tlbnum, u8 index)
{
	struct pvu_hw_tlb_entry *entry;
	struct pvu_hw_tlb *tlb;

	tlb = (struct pvu_hw_tlb *)dev->tlb_base + tlbnum;
	entry = &tlb->entry[index];

	mmio_write32_field(&entry->reg2, PVU_TLB_ENTRY_MODE_MASK,
		PVU_TLB_ENTRY_VALID);

	dev->tlb_data[tlbnum] |= (1 << index);
}

static void pvu_entry_write(struct pvu_dev *dev, u16 tlbnum, u8 index,
			    struct pvu_tlb_entry *ent)
{
	struct pvu_hw_tlb_entry *entry;
	struct pvu_hw_tlb *tlb;
	u8 pgsz;

	tlb = (struct pvu_hw_tlb *)dev->tlb_base + tlbnum;
	entry = &tlb->entry[index];

	for (pgsz = 0; pgsz < ARRAY_SIZE(pvu_page_size_bytes); pgsz++) {
		if (ent->size == pvu_page_size_bytes[pgsz])
			break;
	}

	mmio_write32(&entry->reg0, ent->virt_addr & 0xffffffff);
	mmio_write32_field(&entry->reg1, 0xffff, (ent->virt_addr >> 32));
	mmio_write32(&entry->reg2, 0x0);

	mmio_write32(&entry->reg4, ent->phys_addr & 0xffffffff);
	mmio_write32_field(&entry->reg5, 0xffff, (ent->phys_addr >> 32));
	mmio_write32(&entry->reg6, 0x0);

	mmio_write32_field(&entry->reg2, PVU_TLB_ENTRY_PGSIZE_MASK, pgsz);
	mmio_write32_field(&entry->reg2, PVU_TLB_ENTRY_FLAG_MASK, ent->flags);

	/* Do we need "DSB NSH" here to make sure all writes are finished? */
	pvu_entry_enable(dev, tlbnum, index);
}

static u32 pvu_init_device(struct pvu_dev *dev, u16 max_virtid)
{
	struct pvu_hw_cfg *cfg;
	unsigned int i;

	cfg = (struct pvu_hw_cfg *)dev->cfg_base;

	dev->num_tlbs = mmio_read32_field(&cfg->config, PVU_CONFIG_NTLB_MASK);
	dev->num_entries =
		mmio_read32_field(&cfg->config, PVU_CONFIG_NENT_MASK);

	if (max_virtid >= dev->num_tlbs) {
		printk("ERROR: PVU: Max virtid(%d) should be less than num_tlbs(%d)\n",
			max_virtid, dev->num_tlbs);
		return -EINVAL;
	}

	dev->max_virtid = max_virtid;
	dev->free_tlb_count = dev->num_tlbs - (max_virtid + 1);

	mmio_write32(&cfg->virtid_map1, 0);
	mmio_write32_field(&cfg->virtid_map2, PVU_MAX_VIRTID_MASK, max_virtid);

	for (i = 0; i < dev->num_tlbs; i++) {

		pvu_tlb_disable(dev, i);
		if (i < dev->max_virtid)
			dev->tlb_data[i] = 0x0 | i << dev->num_entries;
		else
			dev->tlb_data[i] = 0x0;
	}

	/* Enable all types of exceptions */
	mmio_write32(&cfg->exception_logging_disable, 0x0);
	mmio_write32(&cfg->exception_logging_control, 0x0);
	mmio_write32_field(&cfg->enable, PVU_ENABLE_MASK, PVU_ENABLE_EN);
	return 0;
}

static void pvu_shutdown_device(struct pvu_dev *dev)
{
	struct pvu_hw_cfg *cfg;
	unsigned int i;

	cfg = (struct pvu_hw_cfg *)dev->cfg_base;

	for (i = 0; i < dev->num_tlbs; i++) {
		pvu_tlb_flush(dev, i);
	}

	mmio_write32_field(&cfg->enable, PVU_ENABLE_MASK, PVU_ENABLE_DIS);
}

/*
 * Split a memory region into multiple pages, where page size is one of the PVU
 * supported size and the start address is aligned to page size
 */
static int pvu_entrylist_create(u64 ipa, u64 pa, u64 map_size, u64 flags,
				struct pvu_tlb_entry *entlist, u32 num_entries)
{
	u64 page_size, vaddr, paddr;
	unsigned int count;
	s64 size;
	int idx;

	vaddr = ipa;
	paddr = pa;
	size = map_size;
	count  = 0;

	while (size > 0) {

		if (count == num_entries) {
			printk("ERROR: PVU: Need more TLB entries for mapping %llx => %llx with size %llx\n",
				ipa, pa, map_size);
			return -EINVAL;
		}

		/* Try size from largest to smallest */
		for (idx = ARRAY_SIZE(pvu_page_size_bytes) - 1; idx >= 0; idx--) {

			page_size = pvu_page_size_bytes[idx];

			if (is_aligned(vaddr, page_size) &&
			    is_aligned(paddr, page_size) &&
			    (u64)size >= page_size) {

				entlist[count].virt_addr = vaddr;
				entlist[count].phys_addr = paddr;
				entlist[count].size = page_size;
				entlist[count].flags = flags;

				count++;
				vaddr += page_size;
				paddr += page_size;
				size -= page_size;
				break;
			}
		}

		if (idx < 0) {
			printk("ERROR: PVU: Addresses %llx %llx" \
				"aren't aligned to any of the allowed page sizes\n",
				vaddr, paddr);
			return -EINVAL;
		}
	}
	return count;
}

static void pvu_entrylist_sort(struct pvu_tlb_entry *entlist, u32 num_entries)
{
	struct pvu_tlb_entry temp;
	unsigned int i, j;

	for (i = 0; i < num_entries; i++) {
		for (j = i; j < num_entries; j++) {

			if (entlist[i].size < entlist[j].size) {
				temp = entlist[i];
				entlist[i] = entlist[j];
				entlist[j] = temp;
			}
		}
	}
}

static void pvu_iommu_program_entries(struct cell *cell, u8 virtid)
{
	unsigned int inst, i, tlbnum, idx, ent_count;
	struct pvu_tlb_entry *ent, *cell_entries;
	struct pvu_dev *dev;
	int tlb_next;

	cell_entries = cell->arch.iommu_pvu.entries;
	ent_count = cell->arch.iommu_pvu.ent_count;
	if (ent_count == 0 || cell_entries == NULL)
		return;

	/* Program same memory mapping for all of the instances */
	for (inst = 0; inst < pvu_count; inst++) {

		dev = &pvu_units[inst];
		if (pvu_tlb_is_enabled(dev, virtid))
			continue;

		tlbnum = virtid;
		for (i = 0; i < ent_count; i++) {

			ent = &cell_entries[i];
			idx = i % dev->num_entries;

			if (idx == 0 && i >= dev->num_entries) {
				/* Find next available TLB and chain to it */
				tlb_next = pvu_tlb_alloc(dev, virtid);
				pvu_tlb_chain(dev, tlbnum, tlb_next);
				pvu_tlb_enable(dev, tlbnum);
				tlbnum = tlb_next;
			}

			pvu_entry_write(dev, tlbnum, idx, ent);
		}
		pvu_tlb_enable(dev, tlbnum);
	}
}

/*
 * Actual TLB entry programming is deferred till config_commit
 * Only populate the pvu_entries array for now
 */
int pvu_iommu_map_memory(struct cell *cell,
			 const struct jailhouse_memory *mem)
{
	struct pvu_tlb_entry *ent;
	struct pvu_dev *dev;
	unsigned int size;
	u32 tlb_count, flags = 0;
	int ret;

	if (pvu_count == 0 || (mem->flags & JAILHOUSE_MEM_DMA) == 0)
		return 0;

	if (cell->arch.iommu_pvu.ent_count == MAX_PVU_ENTRIES)
		return -ENOMEM;

	if (mem->flags & JAILHOUSE_MEM_READ)
		flags |= (LPAE_PAGE_PERM_UR | LPAE_PAGE_PERM_SR);
	if (mem->flags & JAILHOUSE_MEM_WRITE)
		flags |= (LPAE_PAGE_PERM_UW | LPAE_PAGE_PERM_SW);
	if (mem->flags & JAILHOUSE_MEM_EXECUTE)
		flags |= (LPAE_PAGE_PERM_UX | LPAE_PAGE_PERM_SX);

	flags |= (LPAE_PAGE_MEM_WRITETHROUGH | LPAE_PAGE_OUTER_SHARABLE |
		  LPAE_PAGE_IS_NOALLOC | LPAE_PAGE_OS_NOALLOC);

	ent = &cell->arch.iommu_pvu.entries[cell->arch.iommu_pvu.ent_count];
	size = MAX_PVU_ENTRIES - cell->arch.iommu_pvu.ent_count;

	ret = pvu_entrylist_create(mem->virt_start, mem->phys_start, mem->size,
				   flags, ent, size);
	if (ret < 0)
		return ret;

	/*
	 * Check if there are enough TLBs left for *chaining* to ensure that
	 * pvu_tlb_alloc called from config_commit never fails
	 */
	dev = &pvu_units[0];
	tlb_count = (cell->arch.iommu_pvu.ent_count + ret - 1) /
				dev->num_entries;

	if (tlb_count > dev->free_tlb_count) {
		printk("ERROR: PVU: Mapping this memory needs more TLBs than that are available\n");
		return -EINVAL;
	}

	cell->arch.iommu_pvu.ent_count += ret;
	return 0;
}

int pvu_iommu_unmap_memory(struct cell *cell,
			   const struct jailhouse_memory *mem)
{
	u32 cell_state;

	if (pvu_count == 0 || (mem->flags & JAILHOUSE_MEM_DMA) == 0)
		return 0;

	/*
	 * PVU does not support dynamic unmapping of memory
	 */

	cell_state = cell->comm_page.comm_region.cell_state;

	if (cell_state == JAILHOUSE_CELL_RUNNING ||
	    cell_state == JAILHOUSE_CELL_RUNNING_LOCKED)
		printk("WARNING: PVU cannot unmap memory at runtime for cell %s\n",
			cell->config->name);

	return 0;
}

void pvu_iommu_config_commit(struct cell *cell)
{
	union jailhouse_stream_id virtid;
	unsigned int i;

	if (pvu_count == 0 || !cell)
		return;

	/*
	 * Chaining the TLB entries adds extra latency to translate those
	 * addresses.
	 * Sort the entries in descending order of page sizes to reduce effects
	 * of chaining and thus reducing average translation latency
	 */
	pvu_entrylist_sort(cell->arch.iommu_pvu.entries,
			   cell->arch.iommu_pvu.ent_count);

	for_each_stream_id(virtid, cell->config, i) {
		if (virtid.id > MAX_VIRTID)
			continue;

		pvu_iommu_program_entries(cell, virtid.id);
	}

	cell->arch.iommu_pvu.ent_count = 0;
}

static int pvu_iommu_cell_init(struct cell *cell)
{
	union jailhouse_stream_id virtid;
	unsigned int i;
	struct pvu_dev *dev;

	if (pvu_count == 0)
		return 0;

	cell->arch.iommu_pvu.ent_count = 0;
	cell->arch.iommu_pvu.entries = page_alloc(&mem_pool, 1);
	if (!cell->arch.iommu_pvu.entries)
		return -ENOMEM;

	dev = &pvu_units[0];
	for_each_stream_id(virtid, cell->config, i) {

		if (virtid.id > MAX_VIRTID)
			continue;

		if (pvu_tlb_is_enabled(dev, virtid.id))
			return -EINVAL;
	}
	return 0;
}

static int pvu_iommu_flush_context(u16 virtid)
{
	unsigned int i, tlbnum, next;
	struct pvu_dev *dev;

	for (i = 0; i < pvu_count; i++) {

		dev = &pvu_units[i];
		tlbnum = virtid;

		while (tlbnum) {

			next = pvu_tlb_next(dev, tlbnum);
			pvu_tlb_flush(dev, tlbnum);
			tlbnum = next;
		}
	}
	return 0;
}

static void pvu_iommu_cell_exit(struct cell *cell)
{
	union jailhouse_stream_id virtid;
	unsigned int i;

	if (pvu_count == 0)
		return;

	for_each_stream_id(virtid, cell->config, i) {

		if (virtid.id > MAX_VIRTID)
			continue;

		pvu_iommu_flush_context(virtid.id);
	}

	cell->arch.iommu_pvu.ent_count = 0;
	page_free(&mem_pool, cell->arch.iommu_pvu.entries, 1);
	cell->arch.iommu_pvu.entries = NULL;
}

static int pvu_iommu_init(void)
{
	struct jailhouse_iommu *iommu;
	struct pvu_dev *dev;
	unsigned int i;
	int ret;

	iommu = &system_config->platform_info.iommu_units[0];
	for (i = 0; i < iommu_count_units(); iommu++, i++) {

		if (iommu->type != JAILHOUSE_IOMMU_PVU)
			continue;

		dev = &pvu_units[pvu_count];
		dev->cfg_base = paging_map_device(iommu->base,
					iommu->size);
		dev->tlb_base = paging_map_device(iommu->tipvu.tlb_base,
					iommu->tipvu.tlb_size);

		ret = pvu_init_device(dev, MAX_VIRTID);
		if (ret)
			return ret;

		pvu_count++;
	}

	return pvu_iommu_cell_init(&root_cell);
}

static void pvu_iommu_shutdown(void)
{
	struct pvu_dev *dev;
	unsigned int i;

	pvu_iommu_cell_exit(&root_cell);

	for (i = 0; i < pvu_count; i++) {

		dev = &pvu_units[i];
		pvu_shutdown_device(dev);
	}

}

DEFINE_UNIT_MMIO_COUNT_REGIONS_STUB(pvu_iommu);
DEFINE_UNIT(pvu_iommu, "PVU IOMMU");
