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

#ifndef _JAILHOUSE_ENTRY_H
#define _JAILHOUSE_ENTRY_H

#include <jailhouse/header.h>
#include <jailhouse/types.h>

#include <jailhouse/cell-config.h>

#define EPERM		1
#define ENOENT		2
#define EIO		5
#define E2BIG		7
#define ENOMEM		12
#define EBUSY		16
#define EEXIST		17
#define ENODEV		19
#define EINVAL		22
#define ERANGE		34
#define ENOSYS		38

struct per_cpu;
struct cell;

/**
 * @defgroup Setup Setup Subsystem
 *
 * This subsystem coordinates the handover from Linux to the hypervisor.
 *
 * @{
 */

extern struct jailhouse_header hypervisor_header;

/**
 * Architecture-specific entry point for enabling the hypervisor.
 * @param cpu_id	Logical ID of the calling CPU.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * The functions always returns the same value on each CPU it is invoked on.
 *
 * @note This function has to be called for every configured CPU or the setup
 * will fail.
 *
 * @see entry
 * @see jailhouse_entry
 */
int arch_entry(unsigned int cpu_id);

/**
 * Entry point for enabling the hypervisor.
 * @param cpu_id	Logical ID of the calling CPU.
 * @param cpu_data	Data structure of the calling CPU.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * The functions is called by arch_entry(). It always returns the same value on
 * each CPU it is invoked on.
 *
 * @note This function has to be called for every configured CPU or the setup
 * will fail.
 *
 * @see arch_entry
 */
int entry(unsigned int cpu_id, struct per_cpu *cpu_data);

/**
 * Perform architecture-specific early setup steps.
 *
 * @return 0 on success, negative error code otherwise.
 *
 * @note This is called over the master CPU that performs CPU-unrelated setup
 * steps.
 */
int arch_init_early(void);

/**
 * Perform architecture-specific CPU setup steps.
 * @param cpu_data	Data structure of the calling CPU.
 *
 * @return 0 on success, negative error code otherwise.
 */
int arch_cpu_init(struct per_cpu *cpu_data);

/**
 * Perform architecture-specific activation of the hypervisor mode.
 *
 * @note This function does not return to the caller but rather resumes Linux
 * in guest mode at the point arch_entry() would return to.
 */
void __attribute__((noreturn)) arch_cpu_activate_vmm(void);

/**
 * Perform architecture-specific restoration of the CPU state on setup
 * failures or after disabling the hypervisor.
 * @param cpu_data	Data structure of the calling CPU.
 * @param return_code	Return value to pass to Linux.
 *
 * @note Depending on the architectural implementation, this function may not
 * return to the caller but rather jump to the target Linux context.
 */
void arch_cpu_restore(struct per_cpu *cpu_data, int return_code);

/** @} */
#endif /* !_JAILHOUSE_ENTRY_H */
