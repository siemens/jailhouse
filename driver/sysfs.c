/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include "cell.h"
#include "jailhouse.h"
#include "main.h"
#include "sysfs.h"

#include <jailhouse/hypercall.h>

/* For compatibility with older kernel versions */
#include <linux/version.h>
#include <linux/gfp.h>
#include <linux/stat.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)
#endif /* < 3.11 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)
static ssize_t kobj_attr_show(struct kobject *kobj, struct attribute *attr,
			      char *buf)
{
	struct kobj_attribute *kattr;
	ssize_t ret = -EIO;

	kattr = container_of(attr, struct kobj_attribute, attr);
	if (kattr->show)
		ret = kattr->show(kobj, kattr, buf);
	return ret;
}

static ssize_t kobj_attr_store(struct kobject *kobj, struct attribute *attr,
			       const char *buf, size_t count)
{
	struct kobj_attribute *kattr;
	ssize_t ret = -EIO;

	kattr = container_of(attr, struct kobj_attribute, attr);
	if (kattr->store)
		ret = kattr->store(kobj, kattr, buf, count);
	return ret;
}

static const struct sysfs_ops cell_sysfs_ops = {
	.show	= kobj_attr_show,
	.store	= kobj_attr_store,
};
#define kobj_sysfs_ops cell_sysfs_ops
#endif /* < 3.14 */
/* End of compatibility section - remove as version become obsolete */

static struct kobject *cells_dir;

struct jailhouse_cpu_stats_attr {
	struct kobj_attribute kattr;
	unsigned int code;
};

static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buffer)
{
	struct jailhouse_cpu_stats_attr *stats_attr =
		container_of(attr, struct jailhouse_cpu_stats_attr, kattr);
	unsigned int code = JAILHOUSE_CPU_INFO_STAT_BASE + stats_attr->code;
	struct cell *cell = container_of(kobj, struct cell, kobj);
	unsigned long sum = 0;
	unsigned int cpu;
	int value;

	for_each_cpu(cpu, &cell->cpus_assigned) {
		value = jailhouse_call_arg2(JAILHOUSE_HC_CPU_GET_INFO, cpu,
					    code);
		if (value > 0)
			sum += value;
	}

	return sprintf(buffer, "%lu\n", sum);
}

#define JAILHOUSE_CPU_STATS_ATTR(_name, _code) \
	static struct jailhouse_cpu_stats_attr _name##_attr = { \
		.kattr = __ATTR(_name, S_IRUGO, stats_show, NULL), \
		.code = _code, \
	}

JAILHOUSE_CPU_STATS_ATTR(vmexits_total, JAILHOUSE_CPU_STAT_VMEXITS_TOTAL);
JAILHOUSE_CPU_STATS_ATTR(vmexits_mmio, JAILHOUSE_CPU_STAT_VMEXITS_MMIO);
JAILHOUSE_CPU_STATS_ATTR(vmexits_management,
			 JAILHOUSE_CPU_STAT_VMEXITS_MANAGEMENT);
JAILHOUSE_CPU_STATS_ATTR(vmexits_hypercall,
			 JAILHOUSE_CPU_STAT_VMEXITS_HYPERCALL);
#ifdef CONFIG_X86
JAILHOUSE_CPU_STATS_ATTR(vmexits_pio, JAILHOUSE_CPU_STAT_VMEXITS_PIO);
JAILHOUSE_CPU_STATS_ATTR(vmexits_xapic, JAILHOUSE_CPU_STAT_VMEXITS_XAPIC);
JAILHOUSE_CPU_STATS_ATTR(vmexits_cr, JAILHOUSE_CPU_STAT_VMEXITS_CR);
JAILHOUSE_CPU_STATS_ATTR(vmexits_msr, JAILHOUSE_CPU_STAT_VMEXITS_MSR);
JAILHOUSE_CPU_STATS_ATTR(vmexits_cpuid, JAILHOUSE_CPU_STAT_VMEXITS_CPUID);
JAILHOUSE_CPU_STATS_ATTR(vmexits_xsetbv, JAILHOUSE_CPU_STAT_VMEXITS_XSETBV);
JAILHOUSE_CPU_STATS_ATTR(vmexits_exception,
			 JAILHOUSE_CPU_STAT_VMEXITS_EXCEPTION);
#elif defined(CONFIG_ARM) || defined(CONFIG_ARM64)
JAILHOUSE_CPU_STATS_ATTR(vmexits_maintenance, JAILHOUSE_CPU_STAT_VMEXITS_MAINTENANCE);
JAILHOUSE_CPU_STATS_ATTR(vmexits_virt_irq, JAILHOUSE_CPU_STAT_VMEXITS_VIRQ);
JAILHOUSE_CPU_STATS_ATTR(vmexits_virt_sgi, JAILHOUSE_CPU_STAT_VMEXITS_VSGI);
JAILHOUSE_CPU_STATS_ATTR(vmexits_psci, JAILHOUSE_CPU_STAT_VMEXITS_PSCI);
#ifdef CONFIG_ARM
JAILHOUSE_CPU_STATS_ATTR(vmexits_cp15, JAILHOUSE_CPU_STAT_VMEXITS_CP15);
#endif
#endif

static struct attribute *no_attrs[] = {
	&vmexits_total_attr.kattr.attr,
	&vmexits_mmio_attr.kattr.attr,
	&vmexits_management_attr.kattr.attr,
	&vmexits_hypercall_attr.kattr.attr,
#ifdef CONFIG_X86
	&vmexits_pio_attr.kattr.attr,
	&vmexits_xapic_attr.kattr.attr,
	&vmexits_cr_attr.kattr.attr,
	&vmexits_msr_attr.kattr.attr,
	&vmexits_cpuid_attr.kattr.attr,
	&vmexits_xsetbv_attr.kattr.attr,
	&vmexits_exception_attr.kattr.attr,
#elif defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	&vmexits_maintenance_attr.kattr.attr,
	&vmexits_virt_irq_attr.kattr.attr,
	&vmexits_virt_sgi_attr.kattr.attr,
	&vmexits_psci_attr.kattr.attr,
#ifdef CONFIG_ARM
	&vmexits_cp15_attr.kattr.attr,
#endif
#endif
	NULL
};

static struct attribute_group stats_attr_group = {
	.attrs = no_attrs,
	.name = "statistics"
};

static int print_cpumask(char *buf, size_t size, cpumask_t *mask, bool as_list)
{
	int written;

	if (as_list)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
		written = scnprintf(buf, size, "%*pbl\n",
				    cpumask_pr_args(mask));
	else
		written = scnprintf(buf, size, "%*pb\n",
				    cpumask_pr_args(mask));
#else
		written = cpulist_scnprintf(buf, size, mask);
	else
		written = cpumask_scnprintf(buf, size, mask);
	written += scnprintf(buf + written, size - written, "\n");
#endif

	return written;
}

static int print_failed_cpus(char *buf, size_t size, const struct cell *cell,
			 bool as_list)
{
	cpumask_var_t cpus_failed;
	unsigned int cpu;
	int written;

	if (!zalloc_cpumask_var(&cpus_failed, GFP_KERNEL))
		return -ENOMEM;

	for_each_cpu(cpu, &cell->cpus_assigned)
		if (jailhouse_call_arg2(JAILHOUSE_HC_CPU_GET_INFO, cpu,
					JAILHOUSE_CPU_INFO_STATE) ==
		    JAILHOUSE_CPU_FAILED)
			cpumask_set_cpu(cpu, cpus_failed);

	written = print_cpumask(buf, size, cpus_failed, as_list);

	free_cpumask_var(cpus_failed);

	return written;
}

static ssize_t name_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buffer)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	return sprintf(buffer, "%s\n", cell->name);
}

static ssize_t state_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buffer)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	switch (jailhouse_call_arg1(JAILHOUSE_HC_CELL_GET_STATE, cell->id)) {
	case JAILHOUSE_CELL_RUNNING:
		return sprintf(buffer, "running\n");
	case JAILHOUSE_CELL_RUNNING_LOCKED:
		return sprintf(buffer, "running/locked\n");
	case JAILHOUSE_CELL_SHUT_DOWN:
		return sprintf(buffer, "shut down\n");
	case JAILHOUSE_CELL_FAILED:
		return sprintf(buffer, "failed\n");
	default:
		return sprintf(buffer, "invalid\n");
	}
}

static ssize_t cpus_assigned_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	return print_cpumask(buf, PAGE_SIZE, &cell->cpus_assigned, false);
}

static ssize_t cpus_assigned_list_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	return print_cpumask(buf, PAGE_SIZE, &cell->cpus_assigned, true);
}

static ssize_t cpus_failed_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	return print_failed_cpus(buf, PAGE_SIZE, cell, false);
}

static ssize_t cpus_failed_list_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	return print_failed_cpus(buf, PAGE_SIZE, cell, true);
}

static struct kobj_attribute cell_name_attr = __ATTR_RO(name);
static struct kobj_attribute cell_state_attr = __ATTR_RO(state);
static struct kobj_attribute cell_cpus_assigned_attr =
	__ATTR_RO(cpus_assigned);
static struct kobj_attribute cell_cpus_assigned_list_attr =
	__ATTR_RO(cpus_assigned_list);
static struct kobj_attribute cell_cpus_failed_attr = __ATTR_RO(cpus_failed);
static struct kobj_attribute cell_cpus_failed_list_attr =
	__ATTR_RO(cpus_failed_list);

static struct attribute *cell_attrs[] = {
	&cell_name_attr.attr,
	&cell_state_attr.attr,
	&cell_cpus_assigned_attr.attr,
	&cell_cpus_assigned_list_attr.attr,
	&cell_cpus_failed_attr.attr,
	&cell_cpus_failed_list_attr.attr,
	NULL,
};

static struct kobj_type cell_type = {
	.release = jailhouse_cell_kobj_release,
	.sysfs_ops = &kobj_sysfs_ops,
	.default_attrs = cell_attrs,
};

int jailhouse_sysfs_cell_create(struct cell *cell)
{
	int err;

	err = kobject_init_and_add(&cell->kobj, &cell_type, cells_dir, "%d",
				   cell->id);
	if (err) {
		jailhouse_cell_kobj_release(&cell->kobj);
		return err;
	}

	err = sysfs_create_group(&cell->kobj, &stats_attr_group);
	if (err) {
		kobject_put(&cell->kobj);
		return err;
	}

	return 0;
}

void jailhouse_sysfs_cell_register(struct cell *cell)
{
	kobject_uevent(&cell->kobj, KOBJ_ADD);
}

void jailhouse_sysfs_cell_delete(struct cell *cell)
{
	sysfs_remove_group(&cell->kobj, &stats_attr_group);
	kobject_put(&cell->kobj);
}

static ssize_t console_show(struct device *dev, struct device_attribute *attr,
			    char *buffer)
{
	ssize_t ret;

	if (mutex_lock_interruptible(&jailhouse_lock) != 0)
		return -EINTR;

	ret = jailhouse_console_dump_delta(buffer, 0, NULL);
	/* don't return error if jailhouse is not enabled */
	if (ret == -EAGAIN)
		ret = 0;

	mutex_unlock(&jailhouse_lock);

	return ret;
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
			    char *buffer)
{
	return sprintf(buffer, "%d\n", jailhouse_enabled);
}

static ssize_t info_show(struct device *dev, char *buffer, unsigned int type)
{
	ssize_t result;
	long val = 0;

	if (mutex_lock_interruptible(&jailhouse_lock) != 0)
		return -EINTR;

	if (jailhouse_enabled)
		val = jailhouse_call_arg1(JAILHOUSE_HC_HYPERVISOR_GET_INFO,
					  type);
	if (val >= 0)
		result = sprintf(buffer, "%ld\n", val);
	else
		result = val;

	mutex_unlock(&jailhouse_lock);
	return result;
}

static ssize_t mem_pool_size_show(struct device *dev,
				  struct device_attribute *attr, char *buffer)
{
	return info_show(dev, buffer, JAILHOUSE_INFO_MEM_POOL_SIZE);
}

static ssize_t mem_pool_used_show(struct device *dev,
				  struct device_attribute *attr, char *buffer)
{
	return info_show(dev, buffer, JAILHOUSE_INFO_MEM_POOL_USED);
}

static ssize_t remap_pool_size_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buffer)
{
	return info_show(dev, buffer, JAILHOUSE_INFO_REMAP_POOL_SIZE);
}

static ssize_t remap_pool_used_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buffer)
{
	return info_show(dev, buffer, JAILHOUSE_INFO_REMAP_POOL_USED);
}

static ssize_t core_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	return memory_read_from_buffer(buf, count, &off, hypervisor_mem,
				       attr->size);
}

static DEVICE_ATTR_RO(console);
static DEVICE_ATTR_RO(enabled);
static DEVICE_ATTR_RO(mem_pool_size);
static DEVICE_ATTR_RO(mem_pool_used);
static DEVICE_ATTR_RO(remap_pool_size);
static DEVICE_ATTR_RO(remap_pool_used);

static struct attribute *jailhouse_sysfs_entries[] = {
	&dev_attr_console.attr,
	&dev_attr_enabled.attr,
	&dev_attr_mem_pool_size.attr,
	&dev_attr_mem_pool_used.attr,
	&dev_attr_remap_pool_size.attr,
	&dev_attr_remap_pool_used.attr,
	NULL
};

static struct attribute_group jailhouse_attribute_group = {
	.name = NULL,
	.attrs = jailhouse_sysfs_entries,
};

static struct bin_attribute bin_attr_core = {
	.attr.name = "core",
	.attr.mode = S_IRUSR,
	.read = core_show,
};

int jailhouse_sysfs_core_init(struct device *dev, size_t hypervisor_size)
{
	bin_attr_core.size = hypervisor_size;
	return sysfs_create_bin_file(&dev->kobj, &bin_attr_core);
}

void jailhouse_sysfs_core_exit(struct device *dev)
{
	sysfs_remove_bin_file(&dev->kobj, &bin_attr_core);
}

int jailhouse_sysfs_init(struct device *dev)
{
	int err;

	err = sysfs_create_group(&dev->kobj, &jailhouse_attribute_group);
	if (err)
		return err;

	cells_dir = kobject_create_and_add("cells", &dev->kobj);
	if (!cells_dir) {
		sysfs_remove_group(&dev->kobj, &jailhouse_attribute_group);
		return -ENOMEM;
	}

	return 0;
}

void jailhouse_sysfs_exit(struct device *dev)
{
	kobject_put(cells_dir);
	sysfs_remove_group(&dev->kobj, &jailhouse_attribute_group);
}
