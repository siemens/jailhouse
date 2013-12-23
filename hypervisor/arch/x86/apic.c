/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/processor.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <asm/apic.h>
#include <asm/bitops.h>
#include <asm/fault.h>
#include <asm/spinlock.h>
#include <asm/vmx.h>

bool using_x2apic;

static u8 apic_to_cpu_id[] = { [0 ... APIC_MAX_PHYS_ID] = APIC_INVALID_ID };
static DEFINE_SPINLOCK(wait_lock);
static void *xapic_page;

static struct {
	u32 (*read)(unsigned int reg);
	u32 (*read_id)(void);
	void (*write)(unsigned int reg, u32 val);
	void (*send_ipi)(u32 apic_id, u32 icr_lo);
} apic_ops;

static u32 read_xapic(unsigned int reg)
{
	return *(volatile u32 *)(xapic_page + (reg << 4));
}

static u32 read_xapic_id(void)
{
	return *(volatile u32 *)(xapic_page + (APIC_REG_ID << 4)) >> 24;
}

static void write_xapic(unsigned int reg, u32 val)
{
	*(volatile u32 *)(xapic_page + (reg << 4)) = val;
}

static void send_xapic_ipi(u32 apic_id, u32 icr_lo)
{
	while (read_xapic(APIC_REG_ICR) & APIC_ICR_DS_PENDING)
		cpu_relax();
	*(volatile u32 *)(xapic_page + (APIC_REG_ICR_HI << 4)) = apic_id << 24;
	*(volatile u32 *)(xapic_page + (APIC_REG_ICR << 4)) = icr_lo;
}

static u32 read_x2apic(unsigned int reg)
{
	return read_msr(MSR_X2APIC_BASE + reg);
}

static u32 read_x2apic_id(void)
{
	return read_msr(MSR_X2APIC_BASE + APIC_REG_ID);
}

static void write_x2apic(unsigned int reg, u32 val)
{
	write_msr(MSR_X2APIC_BASE + reg, val);
}

static void send_x2apic_ipi(u32 apic_id, u32 icr_lo)
{
	write_msr(MSR_X2APIC_BASE + APIC_REG_ICR,
		  ((unsigned long)apic_id) << 32 | icr_lo);
}

int phys_processor_id(void)
{
	return apic_ops.read_id();
}

int apic_cpu_init(struct per_cpu *cpu_data)
{
	unsigned int apic_id = phys_processor_id();
	unsigned int cpu_id = cpu_data->cpu_id;
	u32 ldr;

	printk("(APIC ID %d) ", apic_id);

	if (apic_id > APIC_MAX_PHYS_ID)
		return -ERANGE;
	if (apic_to_cpu_id[apic_id] != APIC_INVALID_ID)
		return -EBUSY;
	/* only flat mode with LDR corresponding to logical ID supported */
	if (!using_x2apic) {
		ldr = apic_ops.read(APIC_REG_LDR);
		if (apic_ops.read(APIC_REG_DFR) != 0xffffffff ||
		    (ldr != 0 && ldr != 1UL << (cpu_id + 24)))
			return -EINVAL;
	}

	apic_to_cpu_id[apic_id] = cpu_id;
	cpu_data->apic_id = apic_id;
	return 0;
}

int apic_init(void)
{
	unsigned long apicbase;
	int err;

	apicbase = read_msr(MSR_IA32_APICBASE);

	if (apicbase & APIC_BASE_EXTD) {
		/* set programmatically to enable address fixup */
		apic_ops.read = read_x2apic;
		apic_ops.read_id = read_x2apic_id;
		apic_ops.write = write_x2apic;
		apic_ops.send_ipi = send_x2apic_ipi;
		using_x2apic = true;
	} else if (apicbase & APIC_BASE_EN) {
		xapic_page = page_alloc(&remap_pool, 1);
		if (!xapic_page)
			return -ENOMEM;
		err = page_map_create(hv_page_table, XAPIC_BASE, PAGE_SIZE,
				      (unsigned long)xapic_page,
				      PAGE_DEFAULT_FLAGS | PAGE_FLAG_UNCACHED,
				      PAGE_DEFAULT_FLAGS, PAGE_DIR_LEVELS,
				      PAGE_MAP_NON_COHERENT);
		if (err)
			return err;
		apic_ops.read = read_xapic;
		apic_ops.read_id = read_xapic_id;
		apic_ops.write = write_xapic;
		apic_ops.send_ipi = send_xapic_ipi;
	} else
		return -EIO;

	printk("Using x%sAPIC\n", using_x2apic ? "2" : "");

	return 0;
}

static void apic_send_nmi_ipi(struct per_cpu *target_data)
{
	apic_ops.send_ipi(target_data->apic_id,
			  APIC_ICR_DLVR_NMI |
			  APIC_ICR_DEST_PHYSICAL |
			  APIC_ICR_LV_ASSERT |
			  APIC_ICR_TM_EDGE |
			  APIC_ICR_SH_NONE);
}

void arch_suspend_cpu(unsigned int cpu_id)
{
	struct per_cpu *target_data = per_cpu(cpu_id);
	bool target_stopped;

	spin_lock(&wait_lock);

	target_data->stop_cpu = true;
	target_stopped = target_data->cpu_stopped;

	spin_unlock(&wait_lock);

	if (!target_stopped) {
		apic_send_nmi_ipi(target_data);

		while (!target_data->cpu_stopped)
			cpu_relax();
	}
}

void arch_resume_cpu(unsigned int cpu_id)
{
	/* make any state changes visible before releasing the CPU */
	memory_barrier();

	per_cpu(cpu_id)->stop_cpu = false;
}

/* target cpu has to be stopped */
void arch_reset_cpu(unsigned int cpu_id)
{
	per_cpu(cpu_id)->wait_for_sipi = true;
	per_cpu(cpu_id)->sipi_vector = APIC_BSP_PSEUDO_SIPI;

	arch_resume_cpu(cpu_id);
}

void arch_park_cpu(unsigned int cpu_id)
{
	per_cpu(cpu_id)->init_signaled = true;

	/* make state change visible before signaling the CPU */
	memory_barrier();

	apic_send_nmi_ipi(per_cpu(cpu_id));
}

void arch_shutdown_cpu(unsigned int cpu_id)
{
	arch_suspend_cpu(cpu_id);
	per_cpu(cpu_id)->shutdown_cpu = true;
	arch_resume_cpu(cpu_id);
	/*
	 * Note: The caller has to ensure that the target CPU has enough time
	 * to reach the shutdown position before destroying the code path it
	 * has to take to get there. This can be ensured by bringing the CPU
	 * online again under Linux before cleaning up the hypervisor.
	 */
}

void apic_nmi_handler(struct per_cpu *cpu_data)
{
	vmx_schedule_vmexit(cpu_data);
}

int apic_handle_events(struct per_cpu *cpu_data)
{
	int sipi_vector = -1;

	spin_lock(&wait_lock);

	do {
		if (cpu_data->init_signaled) {
			cpu_data->init_signaled = false;
			cpu_data->wait_for_sipi = true;
			sipi_vector = -1;
			vmx_cpu_park();
			break;
		}

		cpu_data->cpu_stopped = true;

		spin_unlock(&wait_lock);

		while (cpu_data->stop_cpu)
			cpu_relax();

		if (cpu_data->shutdown_cpu) {
			/* disable APIC */
			apic_ops.write(APIC_REG_SPIV, 0);
			vmx_cpu_exit(cpu_data);
			asm volatile("hlt");
		}

		spin_lock(&wait_lock);

		cpu_data->cpu_stopped = false;

		if (cpu_data->wait_for_sipi) {
			cpu_data->wait_for_sipi = false;
			sipi_vector = cpu_data->sipi_vector;
		}
	} while (cpu_data->init_signaled);

	if (cpu_data->flush_caches) {
		cpu_data->flush_caches = false;
		flush_tlb();
		vmx_invept();
	}

	spin_unlock(&wait_lock);

	return sipi_vector;
}

static void apic_validate_ipi_mode(struct per_cpu *cpu_data, u32 lo_val)
{
	switch (lo_val & APIC_ICR_DLVR_MASK) {
	case APIC_ICR_DLVR_INIT:
	case APIC_ICR_DLVR_FIXED:
	case APIC_ICR_DLVR_LOWPRI:
	case APIC_ICR_DLVR_NMI:
	case APIC_ICR_DLVR_SIPI:
		break;
	default:
		panic_printk("FATAL: Unsupported APIC delivery mode, "
			     "ICR.lo=%x\n", lo_val);
		panic_stop(cpu_data);
	}

	switch (lo_val & APIC_ICR_SH_MASK) {
	case APIC_ICR_SH_NONE:
	case APIC_ICR_SH_SELF:
		break;
	default:
		panic_printk("FATAL: Unsupported shorthand, ICR.lo=%x\n",
			     lo_val);
		panic_stop(cpu_data);
	}
}

static void apic_deliver_ipi(struct per_cpu *cpu_data,
			     unsigned int target_cpu_id,
			     u32 orig_icr_hi, u32 icr_lo)
{
	struct per_cpu *target_data;
	bool send_nmi;

	if (target_cpu_id == APIC_INVALID_ID ||
	    !test_bit(target_cpu_id, cpu_data->cell->cpu_set->bitmap)) {
		printk("WARNING: CPU %d specified IPI destination outside "
		       "cell boundaries, ICR.hi=%x\n",
		       cpu_data->cpu_id, orig_icr_hi);
		return;
	}

	target_data = per_cpu(target_cpu_id);

	switch (icr_lo & APIC_ICR_DLVR_MASK) {
	case APIC_ICR_DLVR_NMI:
		/* TODO: must be sent via hypervisor */
		printk("Ignoring NMI IPI\n");
		return;
	case APIC_ICR_DLVR_INIT:
	case APIC_ICR_DLVR_SIPI:
		send_nmi = false;

		spin_lock(&wait_lock);

		if ((icr_lo & APIC_ICR_DLVR_MASK) == APIC_ICR_DLVR_INIT) {
			if (!target_data->wait_for_sipi) {
				target_data->init_signaled = true;
				send_nmi = true;
			}
		} else if (target_data->wait_for_sipi) {
			target_data->sipi_vector =
				icr_lo & APIC_ICR_VECTOR_MASK;
			send_nmi = true;
		}

		spin_unlock(&wait_lock);

		if (send_nmi)
			apic_send_nmi_ipi(target_data);
		return;
	}

	apic_ops.send_ipi(target_data->apic_id, icr_lo);
}

static void apic_deliver_logical_dest_ipi(struct per_cpu *cpu_data,
					  unsigned long dest, u32 lo_val,
					  u32 hi_val)
{
	unsigned int target_cpu_id;
	unsigned int logical_id;
	unsigned int cluster_id;
	unsigned long dest_mask;
	unsigned int apic_id;

	if (using_x2apic) {
		cluster_id = (dest & X2APIC_DEST_CLUSTER_ID_MASK) >>
			X2APIC_DEST_CLUSTER_ID_SHIFT;
		dest_mask = ~(dest & X2APIC_DEST_LOGICAL_ID_MASK);
		while (dest_mask != ~0UL) {
			logical_id = ffz(dest_mask);
			dest_mask |= 1UL << logical_id;
			apic_id = logical_id |
				(cluster_id << X2APIC_CLUSTER_ID_SHIFT);
			target_cpu_id = apic_to_cpu_id[apic_id];
			apic_deliver_ipi(cpu_data, target_cpu_id, hi_val,
					 lo_val);
		}
	} else {
		dest_mask = ~dest;
		while (dest_mask != ~0UL) {
			target_cpu_id = ffz(dest_mask);
			dest_mask |= 1UL << target_cpu_id;
			apic_deliver_ipi(cpu_data, target_cpu_id, hi_val,
					 lo_val);
		}
	}
}

void apic_handle_icr_write(struct per_cpu *cpu_data, u32 lo_val, u32 hi_val)
{
	unsigned int target_cpu_id;
	unsigned long dest;

	apic_validate_ipi_mode(cpu_data, lo_val);

	if ((lo_val & APIC_ICR_SH_MASK) == APIC_ICR_SH_SELF) {
		apic_ops.write(APIC_REG_ICR, (lo_val & APIC_ICR_VECTOR_MASK) |
					     APIC_ICR_DLVR_FIXED |
					     APIC_ICR_TM_EDGE |
					     APIC_ICR_SH_SELF);
		return;
	}

	dest = hi_val;
	if (!using_x2apic)
		dest >>= 24;

	if (lo_val & APIC_ICR_DEST_LOGICAL) {
		lo_val &= ~APIC_ICR_DEST_LOGICAL;
		apic_deliver_logical_dest_ipi(cpu_data, dest, lo_val, hi_val);
	} else {
		target_cpu_id = APIC_INVALID_ID;
		if (dest <= APIC_MAX_PHYS_ID)
			target_cpu_id = apic_to_cpu_id[dest];
		apic_deliver_ipi(cpu_data, target_cpu_id, hi_val, lo_val);
	}
}

unsigned int apic_mmio_access(struct registers *guest_regs,
			      struct per_cpu *cpu_data, unsigned long rip,
			      unsigned long page_table_addr, unsigned int reg,
			      bool is_write)
{
	struct mmio_access access;
	unsigned long val;

	access = mmio_parse(cpu_data, rip, page_table_addr, is_write);
	if (access.inst_len == 0)
		return 0;
	if (access.size != 4) {
		panic_printk("FATAL: Unsupported APIC access width %d\n",
			     access.size);
		return 0;
	}
	if (is_write) {
		val = ((unsigned long *)guest_regs)[access.reg];
		if (reg == APIC_REG_ICR) {
			apic_handle_icr_write(cpu_data, val,
					      apic_ops.read(APIC_REG_ICR_HI));
		} else if (reg == APIC_REG_LDR &&
			 val != 1UL << (cpu_data->cpu_id + 24)) {
			panic_printk("FATAL: Unsupported change to LDR: %x\n",
				     val);
			return 0;
		} else if (reg == APIC_REG_DFR && val != 0xffffffff) {
			panic_printk("FATAL: Unsupported change to DFR: %x\n",
				     val);
			return 0;
		} else
			apic_ops.write(reg, val);
	} else {
		val = apic_ops.read(reg);
		((unsigned long *)guest_regs)[access.reg] = val;
	}
	return access.inst_len;
}

void x2apic_handle_write(struct registers *guest_regs)
{
	u32 reg = guest_regs->rcx;

	if (reg == MSR_X2APIC_SELF_IPI)
		/* TODO: emulate */
		printk("Unhandled x2APIC self IPI write\n");
	else
		apic_ops.write(reg - MSR_X2APIC_BASE, guest_regs->rax);
}

void x2apic_handle_read(struct registers *guest_regs)
{
	u32 reg = guest_regs->rcx;

	guest_regs->rax &= ~0xffffffffUL;
	guest_regs->rax |= apic_ops.read(reg - MSR_X2APIC_BASE);

	guest_regs->rdx &= ~0xffffffffUL;
	if (reg == MSR_X2APIC_ICR)
		guest_regs->rdx |= apic_ops.read(reg - MSR_X2APIC_BASE + 1);
}
