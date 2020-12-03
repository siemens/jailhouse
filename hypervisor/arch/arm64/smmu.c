/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright 2018-2020 NXP
 * Copyright Siemens AG, 2020
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Modified from Linux smmu.c
 */

#include <jailhouse/control.h>
#include <jailhouse/printk.h>
#include <jailhouse/unit.h>
#include <asm/iommu.h>
#include <asm/smmu.h>

#include <jailhouse/cell-config.h>

#define TLB_LOOP_TIMEOUT		1000000

/* SMMU global address space */
#define ARM_SMMU_GR0(smmu)		((smmu)->base)
#define ARM_SMMU_GR1(smmu)		((smmu)->base + (1 << (smmu)->pgshift))

/* Translation context bank */
#define ARM_SMMU_CB(smmu, n)	((smmu)->cb_base + ((n) << (smmu)->pgshift))

/* Configuration Register 0 */
#define ARM_SMMU_GR0_sCR0		0x0
#define sCR0_CLIENTPD			(1 << 0)
#define sCR0_GFRE			(1 << 1)
#define sCR0_GFIE			(1 << 2)
#define sCR0_GCFGFRE			(1 << 4)
#define sCR0_GCFGFIE			(1 << 5)
#define sCR0_USFCFG			(1 << 10)
#define sCR0_VMIDPNE			(1 << 11)
#define sCR0_PTM			(1 << 12)
#define sCR0_FB				(1 << 13)

/* Auxiliary Configuration Register */
#define ARM_SMMU_GR0_sACR		0x10
#define ARM_MMU500_ACTLR_CPRE		(1 << 1)
#define ARM_MMU500_ACR_SMTNMB_TLBEN	(1 << 8)
#define ARM_MMU500_ACR_S2CRB_TLBEN	(1 << 10)
#define ARM_MMU500_ACR_CACHE_LOCK	(1 << 26)

/* Identification registers */
#define ARM_SMMU_GR0_ID0		0x20
#define ID0_S2TS			(1 << 29)
#define ID0_SMS				(1 << 27)
#define ID0_CTTW			(1 << 14)
#define ID0_NUMSIDB(id)			GET_FIELD(id, 12, 9)
#define ID0_NUMSMRG(id)			GET_FIELD(id, 7, 0)

#define ARM_SMMU_GR0_ID1		0x24
#define ID1_PAGESIZE			(1 << 31)
#define ID1_NUMPAGENDXB(id)		GET_FIELD(id, 30, 28)
#define ID1_NUMS2CB(id)			GET_FIELD(id, 23, 16)
#define ID1_NUMCB(id)			GET_FIELD(id, 7, 0)

#define ARM_SMMU_GR0_ID2		0x28
#define ID2_PTFS_4K			(1 << 12)
#define ID2_OAS(id)			GET_FIELD(id, 7, 4)
#define ID2_IAS(id)			GET_FIELD(id, 3, 0)

#define ARM_SMMU_GR0_ID7		0x3c
#define ID7_MAJOR(id)			GET_FIELD(id, 7, 4)

/* Global Fault Status Register */
#define ARM_SMMU_GR0_sGFSR		0x48

/* TLB */
#define ARM_SMMU_GR0_TLBIVMID		0X64
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70
#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define sTLBGSTATUS_GSACTIVE		(1 << 0)

/* Stream Match Register */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_VALID			(1 << 31)
#define SMR_MASK_SHIFT			16
#define SMR_ID_SHIFT			0
/* Ignore upper bit in ID and MASK */
#define SMR_GET_ID(smr)			((smr) & BIT_MASK(14, 0))
/* Mask is already specified from bit 0 in the configuration */
#define SMR_GET_MASK(smr)		((smr) & BIT_MASK(14, 0))

/* Stream-to-Context Register */
#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_EXIDVALID			(1 << 10)

/* Context Bank Attribute Registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define CBAR_VMID_SHIFT			0
#define CBAR_TYPE_SHIFT			16
#define CBAR_TYPE_S2_TRANS		(0 << CBAR_TYPE_SHIFT)

#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))
#define CBA2R_RW64_64BIT		(1 << 0)

/* Stage 1 translation context bank address space */
#define ARM_SMMU_CB_SCTLR		0x0
#define ARM_SMMU_CB_ACTLR		0x4
#define ARM_SMMU_CB_TTBR0		0x20
#define ARM_SMMU_CB_TCR			0x30
#define ARM_SMMU_CB_FSR			0x58

#define SCTLR_CFIE			(1 << 6)
#define SCTLR_CFRE			(1 << 5)
#define SCTLR_AFE			(1 << 2)
#define SCTLR_TRE			(1 << 1)
#define SCTLR_M				(1 << 0)

#define TCR_RES0			(BIT_MASK(31, 23) | BIT_MASK(20, 19))

#define FSR_MULTI			(1 << 31)
#define FSR_SS				(1 << 30)
#define FSR_UUT				(1 << 8)
#define FSR_ASF				(1 << 7)
#define FSR_TLBLKF			(1 << 6)
#define FSR_TLBMCF			(1 << 5)
#define FSR_EF				(1 << 4)
#define FSR_PF				(1 << 3)
#define FSR_AFF				(1 << 2)
#define FSR_TF				(1 << 1)
#define FSR_IGN				(FSR_AFF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)

/* Context Bank Index */
#define S2CR_CBNDX(s2cr)		SET_FIELD((s2cr), 7, 0)
/*  Register type */
#define S2CR_TYPE(s2cr)			SET_FIELD((s2cr), 17, 16)
/* Privileged Attribute Configuration */
#define S2CR_PRIVCFG(s2cr)		SET_FIELD((s2cr), 25, 24)

#define S2CR_TYPE_TRANS			0
#define S2CR_TYPE_FAULT			2

#define S2CR_PRIVCFG_DEFAULT		0

struct arm_smmu_smr {
	u16				mask;
	u16				id;
	bool				valid;
};

struct arm_smmu_device {
	void				*base;
	void				*cb_base;
	unsigned long			pgshift;
	u32				num_context_banks;
	u32				num_mapping_groups;
	struct arm_smmu_smr		*smrs;
};

static struct arm_smmu_device smmu_device[JAILHOUSE_MAX_IOMMU_UNITS];
static unsigned int num_smmu_devices;

#define for_each_smmu(smmu, counter)				\
	for ((smmu) = &smmu_device[0], (counter) = 0;		\
	     (counter) < num_smmu_devices;			\
	     (smmu)++, (counter)++)

static void arm_smmu_write_smr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_smr *smr = smmu->smrs + idx;
	u32 reg = (smr->id << SMR_ID_SHIFT) | (smr->mask << SMR_MASK_SHIFT) |
		(smr->valid ? SMR_VALID : 0);

	mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_SMR(idx), reg);
}

static void arm_smmu_write_s2cr(struct arm_smmu_device *smmu, int idx,
				unsigned int type, unsigned int cbndx)
{
	u32 reg = S2CR_TYPE(type) | S2CR_CBNDX(cbndx) |
		  S2CR_PRIVCFG(S2CR_PRIVCFG_DEFAULT);

	mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_S2CR(idx), reg);
}

/* Wait for any pending TLB invalidations to complete */
static int arm_smmu_tlb_sync_global(struct arm_smmu_device *smmu)
{
	void *base = ARM_SMMU_GR0(smmu);
	unsigned int loop, n;

	mmio_write32(base + ARM_SMMU_GR0_sTLBGSYNC, 0);
	for (loop = 0; loop < TLB_LOOP_TIMEOUT; loop++) {
		if (!(mmio_read32(base + ARM_SMMU_GR0_sTLBGSTATUS) &
		      sTLBGSTATUS_GSACTIVE))
			return 0;
		for (n = 0; n < 1000; n++)
			cpu_relax();
	}
	printk("TLB sync timed out -- SMMU may be deadlocked\n");

	return trace_error(-EINVAL);
}

static void arm_smmu_setup_context_bank(struct arm_smmu_device *smmu,
					struct cell *cell, unsigned int vmid)
{
	/*
	 * We use the cell ID here, one cell use one context.
	 */
	void *cb_base = ARM_SMMU_CB(smmu, vmid);
	void *gr1_base = ARM_SMMU_GR1(smmu);

	/* CBA2R */
	mmio_write32(gr1_base + ARM_SMMU_GR1_CBA2R(vmid), CBA2R_RW64_64BIT);

	/* CBAR */
	mmio_write32(gr1_base + ARM_SMMU_GR1_CBAR(vmid),
		     CBAR_TYPE_S2_TRANS | (vmid << CBAR_VMID_SHIFT));

	/* TCR */
	mmio_write32(cb_base + ARM_SMMU_CB_TCR, VTCR_CELL & ~TCR_RES0);

	/* TTBR0 */
	mmio_write64(cb_base + ARM_SMMU_CB_TTBR0,
		     paging_hvirt2phys(cell->arch.mm.root_table) & TTBR_MASK);

	/* SCTLR */
	mmio_write32(cb_base + ARM_SMMU_CB_SCTLR,
		     SCTLR_CFIE | SCTLR_CFRE | SCTLR_AFE | SCTLR_TRE | SCTLR_M);
}

static void arm_smmu_disable_context_bank(struct arm_smmu_device *smmu, int idx)
{
	mmio_write32(ARM_SMMU_CB(smmu, idx) + ARM_SMMU_CB_SCTLR, 0);
}

static int arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	void *gr0_base = ARM_SMMU_GR0(smmu);
	unsigned int idx;
	u32 reg;

	/* Clear global FSR */
	reg = mmio_read32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_sGFSR);
	mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_sGFSR, reg);

	/*
	 * Reset stream mapping groups: Initial values mark all SMRn as
	 * invalid and all S2CRn as fault until overridden.
	 */
	for (idx = 0; idx < smmu->num_mapping_groups; ++idx) {
		if (smmu->smrs)
			arm_smmu_write_smr(smmu, idx);

		arm_smmu_write_s2cr(smmu, idx, S2CR_TYPE_FAULT, 0);
	}

	/*
	 * Before clearing ARM_MMU500_ACTLR_CPRE, need to
	 * clear CACHE_LOCK bit of ACR first.
	 */
	reg = mmio_read32(gr0_base + ARM_SMMU_GR0_sACR);
	reg &= ~ARM_MMU500_ACR_CACHE_LOCK;

	/*
	 * Allow unmatched Stream IDs to allocate bypass
	 * TLB entries for reduced latency.
	 */
	reg |= ARM_MMU500_ACR_SMTNMB_TLBEN | ARM_MMU500_ACR_S2CRB_TLBEN;
	mmio_write32(gr0_base + ARM_SMMU_GR0_sACR, reg);

	/* Make sure all context banks are disabled and clear CB_FSR */
	for (idx = 0; idx < smmu->num_context_banks; ++idx) {
		void *cb_base = ARM_SMMU_CB(smmu, idx);

		arm_smmu_disable_context_bank(smmu, idx);
		mmio_write32(cb_base + ARM_SMMU_CB_FSR, FSR_FAULT);
		/*
		 * Disable MMU-500's not-particularly-beneficial next-page
		 * prefetcher for the sake of errata #841119 and #826419.
		 */
		reg = mmio_read32(cb_base + ARM_SMMU_CB_ACTLR);
		reg &= ~ARM_MMU500_ACTLR_CPRE;
		mmio_write32(cb_base + ARM_SMMU_CB_ACTLR, reg);
	}

	/* Invalidate the TLB, just in case */
	mmio_write32(gr0_base + ARM_SMMU_GR0_TLBIALLH, 0);
	mmio_write32(gr0_base + ARM_SMMU_GR0_TLBIALLNSNH, 0);
	return arm_smmu_tlb_sync_global(smmu);
}

static int arm_smmu_device_cfg_probe(struct arm_smmu_device *smmu)
{
	void *gr0_base = ARM_SMMU_GR0(smmu);
	u32 id, num_s2_context_banks;
	unsigned long size;

	/* We only support version 2 */
	if (ID7_MAJOR(mmio_read32(gr0_base + ARM_SMMU_GR0_ID7)) != 2)
		return trace_error(-EIO);

	/* Make sure the SMMU is not in use */
	if (!(mmio_read32(gr0_base + ARM_SMMU_GR0_sCR0) & sCR0_CLIENTPD))
		return trace_error(-EBUSY);

	/* ID0 */
	id = mmio_read32(gr0_base + ARM_SMMU_GR0_ID0);

	if (!(id & ID0_S2TS))
		return trace_error(-EIO);

	size = 1 << ID0_NUMSIDB(id);

	if (id & ID0_SMS) {
		size = ID0_NUMSMRG(id);
		if (size == 0)
			return trace_error(-ENODEV);

		/* Zero-initialised to mark as invalid */
		smmu->smrs = page_alloc(&mem_pool, PAGES(size * sizeof(*smmu->smrs)));
		if (!smmu->smrs)
			return -ENOMEM;
		memset(smmu->smrs, 0, PAGES(size * sizeof(*smmu->smrs)));

		printk(" stream matching with %lu SMR groups\n", size);
	}

	smmu->num_mapping_groups = size;

	/* ID1 */
	id = mmio_read32(gr0_base + ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ID1_PAGESIZE) ? 16 : 12;

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 << (ID1_NUMPAGENDXB(id) + 1);
	size <<= smmu->pgshift;
	if (smmu->cb_base != gr0_base + size)
		printk("Warning: SMMU address space size (0x%lx) "
		       "differs from mapped region size (0x%tx)!\n",
		       size * 2, (smmu->cb_base - gr0_base) * 2);

	num_s2_context_banks = ID1_NUMS2CB(id);
	smmu->num_context_banks = ID1_NUMCB(id);
	if (num_s2_context_banks > smmu->num_context_banks)
		return trace_error(-ENODEV);

	printk(" %u context banks (%u stage-2 only)\n",
	       smmu->num_context_banks, num_s2_context_banks);

	/* ID2 */
	id = mmio_read32(gr0_base + ARM_SMMU_GR0_ID2);
	if (ID2_IAS(id) < cpu_parange_encoded)
		return trace_error(-EIO);
	if (ID2_OAS(id) < cpu_parange_encoded)
		return trace_error(-EIO);
	if (!(id & ID2_PTFS_4K))
		return trace_error(-EIO);

	return 0;
}

static int arm_smmu_find_sme(u16 id, u16 mask, struct arm_smmu_device *smmu)
{
	struct arm_smmu_smr *smrs = smmu->smrs;
	int free_idx = -EINVAL;
	unsigned int n;

	/* Stream indexing is blissfully easy */
	if (!smrs)
		return id;

	/* Validating SMRs is... less so */
	for (n = 0; n < smmu->num_mapping_groups; ++n) {
		if (!smrs[n].valid) {
			/*
			 * Note the first free entry we come across, which
			 * we'll claim in the end if nothing else matches.
			 */
			if (free_idx < 0)
				free_idx = n;
			continue;
		}
		/*
		 * If the new entry is _entirely_ matched by an existing entry,
		 * then reuse that, with the guarantee that there also cannot
		 * be any subsequent conflicting entries. In normal use we'd
		 * expect simply identical entries for this case, but there's
		 * no harm in accommodating the generalisation.
		 */
		if ((mask & smrs[n].mask) == mask &&
		    !((id ^ smrs[n].id) & ~smrs[n].mask)) {
			return n;
		}
		/*
		 * If the new entry has any other overlap with an existing one,
		 * though, then there always exists at least one stream ID
		 * which would cause a conflict, and we can't allow that risk.
		 */
		if (!((id ^ smrs[n].id) & ~(smrs[n].mask | mask)))
			return -EINVAL;
	}

	return free_idx;
}

static int arm_smmu_cell_init(struct cell *cell)
{
	unsigned int vmid = cell->config->id;
	struct arm_smmu_device *smmu;
	struct arm_smmu_smr *smr;
	union jailhouse_stream_id fsid;
	unsigned int dev, n;
	u16 sid, smask;
	int ret, idx;

	/* If no sids, ignore */
	if (!cell->config->num_stream_ids)
		return 0;

	for_each_smmu(smmu, dev) {
		arm_smmu_setup_context_bank(smmu, cell, vmid);

		smr = smmu->smrs;

		for_each_stream_id(fsid, cell->config, n) {
			sid = SMR_GET_ID(fsid.mmu500.id);
			smask = SMR_GET_MASK(fsid.mmu500.mask_out);

			ret = arm_smmu_find_sme(sid, smask, smmu);
			if (ret < 0)
				return trace_error(ret);
			idx = ret;

			printk("Assigning SID 0x%x, Mask 0x%x to cell \"%s\"\n",
			       sid, smask, cell->config->name);

			arm_smmu_write_s2cr(smmu, idx, S2CR_TYPE_TRANS, vmid);

			smr[idx].id = sid;
			smr[idx].mask = smask;
			smr[idx].valid = true;

			arm_smmu_write_smr(smmu, idx);
		}

		mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_TLBIVMID, vmid);
		ret = arm_smmu_tlb_sync_global(smmu);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static bool arm_smmu_return_sid_to_root_cell(struct arm_smmu_device *smmu,
					     union jailhouse_stream_id fsid,
					     int idx)
{
	unsigned int n;
	union jailhouse_stream_id rsid;

	for_each_stream_id(rsid, root_cell.config, n) {
		if (fsid.id == rsid.id) {
			printk("Assigning SID 0x%llx Mask: 0x%llx to cell \"%s\"\n",
			       SMR_GET_ID(fsid.mmu500.id),
			       SMR_GET_MASK(fsid.mmu500.mask_out),
			       root_cell.config->name);

			/* We just need to update S2CR, SMR can stay as is. */
			arm_smmu_write_s2cr(smmu, idx, S2CR_TYPE_TRANS,
					    root_cell.config->id);
			return true;
		}
	}
	return false;
}

static void arm_smmu_cell_exit(struct cell *cell)
{
	int id = cell->config->id;
	struct arm_smmu_device *smmu;
	union jailhouse_stream_id fsid;
	unsigned int dev, n;
	u16 sid, smask;
	int idx;

	/* If no sids, ignore */
	if (!cell->config->num_stream_ids)
		return;

	for_each_smmu(smmu, dev) {
		for_each_stream_id(fsid, cell->config, n) {
			sid = SMR_GET_ID(fsid.mmu500.id);
			smask = SMR_GET_MASK(fsid.mmu500.mask_out);

			idx = arm_smmu_find_sme(sid, smask, smmu);
			if (idx < 0)
				continue;

			/* return full stream ids */
			if (arm_smmu_return_sid_to_root_cell(smmu, fsid, idx))
				continue;

			if (smmu->smrs) {
				smmu->smrs[idx].id = 0;
				smmu->smrs[idx].mask = 0;
				smmu->smrs[idx].valid = false;

				arm_smmu_write_smr(smmu, idx);
			}
			arm_smmu_write_s2cr(smmu, idx, S2CR_TYPE_FAULT, 0);
		}

		arm_smmu_disable_context_bank(smmu, id);

		mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_TLBIVMID, id);
		arm_smmu_tlb_sync_global(smmu);
	}
}

void arm_smmu_config_commit(struct cell *cell)
{
	struct arm_smmu_device *smmu;
	unsigned int dev;

	if (cell != &root_cell)
		return;

	for_each_smmu(smmu, dev) {
		/*
		 * Enable fault reporting,
		 * private VMIDS, disable TLB broadcasting,
		 * fault unmatched streams
		 */
		mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_sCR0,
			sCR0_GFRE | sCR0_GFIE | sCR0_GCFGFRE | sCR0_GCFGFIE |
			sCR0_VMIDPNE | sCR0_PTM | sCR0_USFCFG);
	}
}

static void arm_smmu_shutdown(void)
{
	struct arm_smmu_device *smmu;
	unsigned int dev;

	for_each_smmu(smmu, dev) {
		mmio_write32(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_sCR0,
			     sCR0_CLIENTPD);
	}
}

static int arm_smmu_init(void)
{
	struct jailhouse_iommu *iommu;
	struct arm_smmu_device *smmu;
	unsigned int n;
	int err;

	for (n = 0; n < iommu_count_units(); n++) {
		iommu = &system_config->platform_info.iommu_units[n];
		if (iommu->type != JAILHOUSE_IOMMU_ARM_MMU500)
			continue;

		smmu = &smmu_device[num_smmu_devices];
		smmu->base = paging_map_device(iommu->base, iommu->size);
		if (!smmu->base) {
			err = -ENOMEM;
			goto error;
		}

		printk("ARM MMU500 at 0x%llx with:\n", iommu->base);

		smmu->cb_base = smmu->base + iommu->size / 2;

		err = arm_smmu_device_cfg_probe(smmu);
		if (err)
			goto error;

		err = arm_smmu_device_reset(smmu);
		if (err)
			goto error;

		num_smmu_devices++;
	}

	if (num_smmu_devices == 0)
		return 0;

	err = arm_smmu_cell_init(&root_cell);
	if (!err)
		return 0;

error:
	arm_smmu_shutdown();
	return err;
}

DEFINE_UNIT_MMIO_COUNT_REGIONS_STUB(arm_smmu);
DEFINE_UNIT(arm_smmu, "ARM SMMU");
