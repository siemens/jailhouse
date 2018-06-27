/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/jailhouse_header.h>

#define JAILHOUSE_SIGNATURE	"JAILHOUS"

#define HYP_STUB_ABI_LEGACY 0
#define HYP_STUB_ABI_OPCODE 1


#ifdef __ASSEMBLY__

#define __JH_CONST_UL(x)	x

#else /* !__ASSEMBLY__ */

#define __JH_CONST_UL(x)	x ## UL

/**
 * @ingroup Setup
 * @{
 */

/**
 * Hypervisor entry point.
 *
 * @see arch_entry
 */
typedef int (*jailhouse_entry)(unsigned int);

struct jailhouse_virt_console {
	unsigned int busy;
	unsigned int tail;
	/* current implementation requires the size of the content to be a
	 * power of two */
	char content[2048];
};

/**
 * Hypervisor description.
 * Located at the beginning of the hypervisor binary image and loaded by
 * the driver (which also initializes some fields).
 */
struct jailhouse_header {
	/** Signature "JAILHOUS" used for basic validity check of the
	 * hypervisor image.
	 * @note Filled at build time. */
	char signature[8];
	/** Size of hypervisor core.
	 * It starts with the hypervisor's header and ends after its bss
	 * section. Rounded up to page boundary.
	 * @note Filled at build time. */
	unsigned long core_size;
	/** Size of the per-CPU data structure.
	 * @note Filled at build time. */
	unsigned long percpu_size;
	/** Entry point (arch_entry()).
	 * @note Filled at build time. */
	int (*entry)(unsigned int);
	/** Offset of the console page inside the hypervisor memory
	 * @note Filled at build time. */
	unsigned long console_page;
	/** Pointer to the first struct gcov_info
	 * @note Filled at build time */
	void *gcov_info_head;

	/** Configured maximum logical CPU ID + 1.
	 * @note Filled by Linux loader driver before entry. */
	unsigned int max_cpus;
	/** Number of online CPUs that will call the entry function.
	 * @note Filled by Linux loader driver before entry. */
	unsigned int online_cpus;
	/** Virtual base address of the debug console device (if used).
	 * @note Filled by Linux loader driver on ARM and x86 before entry.
	 *       Filled by arch_entry on ARM64. */
	void *debug_console_base;

	/** Physical address of Linux's hyp-stubs.
	 * @note Filled by Linux loader driver before entry. */
	unsigned long long arm_linux_hyp_vectors;
	/** Denotes hyp-stub ABI for arm and arm64:
	 * @note Filled by Linux loader driver before entry. */
	unsigned int arm_linux_hyp_abi;
};

#endif /* !__ASSEMBLY__ */
