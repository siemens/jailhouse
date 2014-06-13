/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) ARM Limited, 2014
 *
 * Authors:
 *  Jean-Philippe Brucker <jean-philippe.brucker@arm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/setup.h>
#include <asm/setup_mmu.h>
#include <asm/sysregs.h>
#include <jailhouse/paging.h>

/*
 * Jumping to EL2 in the same C code represents an interesting challenge, since
 * it will switch from virtual addresses to physical ones, and then back to
 * virtual after setting up the EL2 MMU.
 */
int switch_exception_level(struct per_cpu *cpu_data)
{
	extern unsigned long bootstrap_vectors;
	extern unsigned long hyp_vectors;

	/* Save the virtual address of the phys2virt function for later */
	phys2virt_t phys2virt = paging_phys2hvirt;
	virt2phys_t virt2phys = paging_hvirt2phys;
	unsigned long phys_bootstrap = virt2phys(&bootstrap_vectors);

	cpu_switch_el2(phys_bootstrap, virt2phys);
	/*
	 * At this point, we are at EL2, and we work with physical addresses.
	 * The MMU needs to be initialised and execution must go back to virtual
	 * addresses before returning, or else we are pretty much doomed.
	 */

	/* Set the new vectors once we're back to a sane, virtual state */
	arm_write_sysreg(HVBAR, &hyp_vectors);

	return 0;
}
