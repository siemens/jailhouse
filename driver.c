/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013, 2014
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/firmware.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/pci.h>
#include <asm/smp.h>
#include <asm/cacheflush.h>

#include "jailhouse.h"
#include <jailhouse/cell-config.h>
#include <jailhouse/header.h>
#include <jailhouse/hypercall.h>
#include <jailhouse/version.h>

#ifdef CONFIG_X86_32
#error 64-bit kernel required!
#endif

#if JAILHOUSE_CELL_ID_NAMELEN != JAILHOUSE_CELL_NAME_MAXLEN
# warning JAILHOUSE_CELL_ID_NAMELEN and JAILHOUSE_CELL_NAME_MAXLEN out of sync!
#endif

/* For compatibility with older kernel versions */
#include <linux/version.h>

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

#ifdef CONFIG_X86
#define JAILHOUSE_AMD_FW_NAME	"jailhouse-amd.bin"
#define JAILHOUSE_INTEL_FW_NAME	"jailhouse-intel.bin"
#else
#define JAILHOUSE_FW_NAME	"jailhouse.bin"
#endif

struct cell {
	struct kobject kobj;
	struct list_head entry;
	unsigned int id;
	cpumask_t cpus_assigned;
	u32 num_memory_regions;
	u32 num_pci_devices;
	struct jailhouse_memory *memory_regions;
	struct jailhouse_pci_device *pci_devices;
};

MODULE_DESCRIPTION("Loader for Jailhouse partitioning hypervisor");
MODULE_LICENSE("GPL");
#ifdef CONFIG_X86
MODULE_FIRMWARE(JAILHOUSE_AMD_FW_NAME);
MODULE_FIRMWARE(JAILHOUSE_INTEL_FW_NAME);
#else
MODULE_FIRMWARE(JAILHOUSE_FW_NAME);
#endif
MODULE_VERSION(JAILHOUSE_VERSION);

static struct device *jailhouse_dev;
static DEFINE_MUTEX(lock);
static bool enabled;
static void *hypervisor_mem;
static unsigned long hv_core_and_percpu_size;
static cpumask_t offlined_cpus;
static atomic_t call_done;
static int error_code;
static LIST_HEAD(cells);
static struct cell *root_cell;
static struct kobject *cells_dir;

#define MIN(a, b)	((a) < (b) ? (a) : (b))

#ifdef CONFIG_X86
bool jailhouse_use_vmcall;

static void init_hypercall(void)
{
	jailhouse_use_vmcall = boot_cpu_has(X86_FEATURE_VMX);
}
#else /* !CONFIG_X86 */
static void init_hypercall(void)
{
}
#endif

enum { JAILHOUSE_PCI_ACTION_ADD, JAILHOUSE_PCI_ACTION_DEL };

#ifdef CONFIG_PCI
static void jailhouse_pci_add_device(const struct jailhouse_pci_device *dev)
{
	int num;
	struct pci_bus *bus;

	bus = pci_find_bus(dev->domain, PCI_BUS_NUM(dev->bdf));
	if (bus) {
		num = pci_scan_slot(bus, dev->bdf & 0xff);
		if (num) {
			pci_lock_rescan_remove();
			pci_bus_assign_resources(bus);
			pci_bus_add_devices(bus);
			pci_unlock_rescan_remove();
		}
	}
}

static void jailhouse_pci_remove_device(const struct jailhouse_pci_device *dev)
{
	struct pci_dev *l_dev;

	l_dev = pci_get_bus_and_slot(PCI_BUS_NUM(dev->bdf), dev->bdf & 0xff);
	if (l_dev)
		pci_stop_and_remove_bus_device_locked(l_dev);
}

static void jailhouse_pci_do_all_devices(struct cell *cell, unsigned int type,
					 unsigned int action)
{
	unsigned int n;
	const struct jailhouse_pci_device *dev;

	dev = cell->pci_devices;
	for (n = cell->num_pci_devices; n > 0; n--) {
		if (dev->type == type) {
			if (action == JAILHOUSE_PCI_ACTION_ADD)
				jailhouse_pci_add_device(dev);
			else if (action == JAILHOUSE_PCI_ACTION_DEL)
				jailhouse_pci_remove_device(dev);
		}
		dev++;
	}
}
#else /* CONFIG_PCI */
static void jailhouse_pci_do_all_devices(struct cell *cell, unsigned int type,
					 unsigned int action)
{
}
#endif /* CONFIG_PCI */

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
#elif defined(CONFIG_ARM)
JAILHOUSE_CPU_STATS_ATTR(vmexits_maintenance, JAILHOUSE_CPU_STAT_VMEXITS_MAINTENANCE);
JAILHOUSE_CPU_STATS_ATTR(vmexits_virt_irq, JAILHOUSE_CPU_STAT_VMEXITS_VIRQ);
JAILHOUSE_CPU_STATS_ATTR(vmexits_virt_sgi, JAILHOUSE_CPU_STAT_VMEXITS_VSGI);
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
#elif defined(CONFIG_ARM)
	&vmexits_maintenance_attr.kattr.attr,
	&vmexits_virt_irq_attr.kattr.attr,
	&vmexits_virt_sgi_attr.kattr.attr,
#endif
	NULL
};

static struct attribute_group stats_attr_group = {
	.attrs = no_attrs,
	.name = "statistics"
};

static ssize_t id_show(struct kobject *kobj, struct kobj_attribute *attr,
		       char *buffer)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	return sprintf(buffer, "%u\n", cell->id);
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
	int written;

	written = cpumask_scnprintf(buf, PAGE_SIZE, &cell->cpus_assigned);
	written += scnprintf(buf + written, PAGE_SIZE - written, "\n");
	return written;
}

static ssize_t cpus_failed_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);
	cpumask_var_t cpus_failed;
	unsigned int cpu;
	int written;

	if (!zalloc_cpumask_var(&cpus_failed, GFP_KERNEL))
		return -ENOMEM;

	for_each_cpu(cpu, &cell->cpus_assigned)
		if (jailhouse_call_arg2(JAILHOUSE_HC_CPU_GET_INFO, cpu,
					JAILHOUSE_CPU_INFO_STATE) ==
		    JAILHOUSE_CPU_FAILED)
			cpu_set(cpu, *cpus_failed);

	written = cpumask_scnprintf(buf, PAGE_SIZE, cpus_failed);
	written += scnprintf(buf + written, PAGE_SIZE - written, "\n");

	free_cpumask_var(cpus_failed);

	return written;
}

static struct kobj_attribute cell_id_attr = __ATTR_RO(id);
static struct kobj_attribute cell_state_attr = __ATTR_RO(state);
static struct kobj_attribute cell_cpus_assigned_attr =
	__ATTR_RO(cpus_assigned);
static struct kobj_attribute cell_cpus_failed_attr = __ATTR_RO(cpus_failed);

static struct attribute *cell_attrs[] = {
	&cell_id_attr.attr,
	&cell_state_attr.attr,
	&cell_cpus_assigned_attr.attr,
	&cell_cpus_failed_attr.attr,
	NULL,
};

static void cell_kobj_release(struct kobject *kobj)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	vfree(cell->memory_regions);
	vfree(cell->pci_devices);
	kfree(cell);
}

static struct kobj_type cell_type = {
	.release = cell_kobj_release,
	.sysfs_ops = &kobj_sysfs_ops,
	.default_attrs = cell_attrs,
};

static struct cell *create_cell(const struct jailhouse_cell_desc *cell_desc)
{
	struct cell *cell;
	int err;

	cell = kzalloc(sizeof(*cell), GFP_KERNEL);
	if (!cell)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&cell->entry);

	bitmap_copy(cpumask_bits(&cell->cpus_assigned),
		    jailhouse_cell_cpu_set(cell_desc),
		    MIN(nr_cpumask_bits, cell_desc->cpu_set_size * 8));

	cell->num_memory_regions = cell_desc->num_memory_regions;
	cell->memory_regions = vmalloc(sizeof(struct jailhouse_memory) *
				       cell->num_memory_regions);
	if (!cell->memory_regions) {
		kfree(cell);
		return ERR_PTR(-ENOMEM);
	}

	memcpy(cell->memory_regions, jailhouse_cell_mem_regions(cell_desc),
	       sizeof(struct jailhouse_memory) * cell->num_memory_regions);
#ifdef CONFIG_PCI
	cell->num_pci_devices = cell_desc->num_pci_devices;
	cell->pci_devices = vmalloc(sizeof(struct jailhouse_pci_device) *
				    cell->num_pci_devices);
	if (cell->num_pci_devices && !cell->pci_devices) {
		vfree(cell->memory_regions);
		kfree(cell);
		return ERR_PTR(-ENOMEM);
	}

	memcpy(cell->pci_devices, jailhouse_cell_pci_devices(cell_desc),
	       sizeof(struct jailhouse_pci_device) * cell->num_pci_devices);
#endif
	err = kobject_init_and_add(&cell->kobj, &cell_type, cells_dir, "%s",
				   cell_desc->name);
	if (err) {
		cell_kobj_release(&cell->kobj);
		return ERR_PTR(err);
	}

	err = sysfs_create_group(&cell->kobj, &stats_attr_group);
	if (err) {
		kobject_put(&cell->kobj);
		return ERR_PTR(err);
	}

	return cell;
}

static void register_cell(struct cell *cell)
{
	list_add_tail(&cell->entry, &cells);
	kobject_uevent(&cell->kobj, KOBJ_ADD);
}

static struct cell *find_cell(struct jailhouse_cell_id *cell_id)
{
	struct cell *cell;

	list_for_each_entry(cell, &cells, entry)
		if (cell_id->id == cell->id ||
		    (cell_id->id == JAILHOUSE_CELL_ID_UNUSED &&
		     strcmp(kobject_name(&cell->kobj), cell_id->name) == 0))
			return cell;
	return NULL;
}

static void delete_cell(struct cell *cell)
{
	list_del(&cell->entry);
	sysfs_remove_group(&cell->kobj, &stats_attr_group);
	kobject_put(&cell->kobj);
}

static long get_max_cpus(u32 cpu_set_size,
			 const struct jailhouse_system __user *system_config)
{
	u8 __user *cpu_set =
		(u8 __user *)jailhouse_cell_cpu_set(&system_config->root_cell);
	unsigned int pos = cpu_set_size;
	long max_cpu_id;
	u8 bitmap;

	while (pos-- > 0) {
		if (get_user(bitmap, cpu_set + pos))
			return -EFAULT;
		max_cpu_id = fls(bitmap);
		if (max_cpu_id > 0)
			return pos * 8 + max_cpu_id;
	}
	return -EINVAL;
}

static void *jailhouse_ioremap(phys_addr_t phys, unsigned long virt,
			       unsigned long size)
{
	struct vm_struct *vma;

	size = PAGE_ALIGN(size);
	if (virt)
		vma = __get_vm_area(size, VM_IOREMAP, virt,
				    virt + size + PAGE_SIZE);
	else
		vma = __get_vm_area(size, VM_IOREMAP, VMALLOC_START,
				    VMALLOC_END);
	if (!vma)
		return NULL;
	vma->phys_addr = phys;

	if (ioremap_page_range((unsigned long)vma->addr,
			       (unsigned long)vma->addr + size, phys,
			       PAGE_KERNEL_EXEC)) {
		vunmap(vma->addr);
		return NULL;
	}

	return vma->addr;
}

static void enter_hypervisor(void *info)
{
	struct jailhouse_header *header = info;
	unsigned int cpu = smp_processor_id();
	int err;

	if (cpu < header->max_cpus)
		/* either returns 0 or the same error code across all CPUs */
		err = header->entry(cpu);
	else
		err = -EINVAL;

	if (err)
		error_code = err;

	atomic_inc(&call_done);
}

static inline const char * jailhouse_fw_name(void)
{
#ifdef CONFIG_X86
	if (boot_cpu_has(X86_FEATURE_SVM))
		return JAILHOUSE_AMD_FW_NAME;
	if (boot_cpu_has(X86_FEATURE_VMX))
		return JAILHOUSE_INTEL_FW_NAME;
	return NULL;
#else
	return JAILHOUSE_FW_NAME;
#endif
}

static int jailhouse_enable(struct jailhouse_system __user *arg)
{
	const struct firmware *hypervisor;
	struct jailhouse_system config_header;
	struct jailhouse_system *config;
	struct jailhouse_memory *hv_mem = &config_header.hypervisor_memory;
	struct jailhouse_header *header;
	void __iomem *uart = NULL;
	unsigned long config_size;
	const char *fw_name;
	long max_cpus;
	int err;

	fw_name = jailhouse_fw_name();
	if (!fw_name) {
		pr_err("jailhouse: Missing or unsupported HVM technology\n");
		return -ENODEV;
	}

	if (copy_from_user(&config_header, arg, sizeof(config_header)))
		return -EFAULT;
	config_header.root_cell.name[JAILHOUSE_CELL_NAME_MAXLEN] = 0;

	max_cpus = get_max_cpus(config_header.root_cell.cpu_set_size, arg);
	if (max_cpus < 0)
		return max_cpus;
	if (max_cpus > UINT_MAX)
		return -EINVAL;

	if (mutex_lock_interruptible(&lock) != 0)
		return -EINTR;

	err = -EBUSY;
	if (enabled || !try_module_get(THIS_MODULE))
		goto error_unlock;

	err = request_firmware(&hypervisor, fw_name, jailhouse_dev);
	if (err) {
		pr_err("jailhouse: Missing hypervisor image %s\n", fw_name);
		goto error_put_module;
	}

	header = (struct jailhouse_header *)hypervisor->data;

	err = -EINVAL;
	if (memcmp(header->signature, JAILHOUSE_SIGNATURE,
		   sizeof(header->signature)) != 0)
		goto error_release_fw;

	hv_core_and_percpu_size = PAGE_ALIGN(header->core_size) +
		max_cpus * header->percpu_size;
	config_size = jailhouse_system_config_size(&config_header);
	if (hv_mem->size <= hv_core_and_percpu_size + config_size)
		goto error_release_fw;

	hypervisor_mem = jailhouse_ioremap(hv_mem->phys_start, JAILHOUSE_BASE,
					   hv_mem->size);
	if (!hypervisor_mem) {
		pr_err("jailhouse: Unable to map RAM reserved for hypervisor "
		       "at %08lx\n", (unsigned long)hv_mem->phys_start);
		goto error_release_fw;
	}

	memcpy(hypervisor_mem, hypervisor->data, hypervisor->size);
	memset(hypervisor_mem + hypervisor->size, 0,
	       hv_mem->size - hypervisor->size);

	header = (struct jailhouse_header *)hypervisor_mem;
	header->max_cpus = max_cpus;

	config = (struct jailhouse_system *)
		(hypervisor_mem + hv_core_and_percpu_size);
	if (copy_from_user(config, arg, config_size)) {
		err = -EFAULT;
		goto error_unmap;
	}

	if (config->debug_uart.flags & JAILHOUSE_MEM_IO) {
		uart = ioremap(config->debug_uart.phys_start,
			       config->debug_uart.size);
		if (!uart) {
			err = -EINVAL;
			pr_err("jailhouse: Unable to map hypervisor UART at "
			       "%08lx\n",
			       (unsigned long)config->debug_uart.phys_start);
			goto error_unmap;
		}
		header->debug_uart_base = (void *)uart;
	}

	root_cell = create_cell(&config->root_cell);
	if (IS_ERR(root_cell)) {
		err = PTR_ERR(root_cell);
		goto error_unmap;
	}

	cpumask_and(&root_cell->cpus_assigned, &root_cell->cpus_assigned,
		    cpu_online_mask);

	error_code = 0;

	preempt_disable();

	header->online_cpus = num_online_cpus();

	atomic_set(&call_done, 0);
	on_each_cpu(enter_hypervisor, header, 0);
	while (atomic_read(&call_done) != num_online_cpus())
		cpu_relax();

	preempt_enable();

	if (error_code) {
		err = error_code;
		goto error_free_cell;
	}

	jailhouse_pci_do_all_devices(root_cell, JAILHOUSE_PCI_TYPE_IVSHMEM,
				     JAILHOUSE_PCI_ACTION_ADD);

	if (uart)
		iounmap(uart);

	release_firmware(hypervisor);

	enabled = true;
	root_cell->id = 0;
	register_cell(root_cell);

	mutex_unlock(&lock);

	pr_info("The Jailhouse is opening.\n");

	return 0;

error_free_cell:
	delete_cell(root_cell);

error_unmap:
	vunmap(hypervisor_mem);
	if (uart)
		iounmap(uart);

error_release_fw:
	release_firmware(hypervisor);

error_put_module:
	module_put(THIS_MODULE);

error_unlock:
	mutex_unlock(&lock);
	return err;
}

static void leave_hypervisor(void *info)
{
	unsigned long size;
	void *page;
	int err;

	/* Touch each hypervisor page we may need during the switch so that
	 * the active mm definitely contains all mappings. At least x86 does
	 * not support taking any faults while switching worlds. */
	for (page = hypervisor_mem, size = hv_core_and_percpu_size; size > 0;
	     size -= PAGE_SIZE, page += PAGE_SIZE)
		readl(page);

	/* either returns 0 or the same error code across all CPUs */
	err = jailhouse_call(JAILHOUSE_HC_DISABLE);
	if (err)
		error_code = err;

	atomic_inc(&call_done);
}

static int jailhouse_disable(void)
{
	struct cell *cell, *tmp;
	unsigned int cpu;
	int err;

	if (mutex_lock_interruptible(&lock) != 0)
		return -EINTR;

	if (!enabled) {
		err = -EINVAL;
		goto unlock_out;
	}

	error_code = 0;

	preempt_disable();

	atomic_set(&call_done, 0);
	on_each_cpu(leave_hypervisor, NULL, 0);
	while (atomic_read(&call_done) != num_online_cpus())
		cpu_relax();

	preempt_enable();

	err = error_code;
	if (err)
		goto unlock_out;

	vunmap(hypervisor_mem);

	for_each_cpu(cpu, &offlined_cpus) {
		if (cpu_up(cpu) != 0)
			pr_err("Jailhouse: failed to bring CPU %d back "
			       "online\n", cpu);
		cpu_clear(cpu, offlined_cpus);
	}

	jailhouse_pci_do_all_devices(root_cell, JAILHOUSE_PCI_TYPE_IVSHMEM,
				     JAILHOUSE_PCI_ACTION_DEL);

	list_for_each_entry_safe(cell, tmp, &cells, entry)
		delete_cell(cell);
	enabled = false;
	module_put(THIS_MODULE);

	pr_info("The Jailhouse was closed.\n");

unlock_out:
	mutex_unlock(&lock);

	return err;
}

static int jailhouse_cell_create(struct jailhouse_cell_create __user *arg)
{
	struct jailhouse_cell_create cell_params;
	struct jailhouse_cell_desc *config;
	struct jailhouse_cell_id cell_id;
	struct cell *cell;
	unsigned int cpu;
	int id, err;

	if (copy_from_user(&cell_params, arg, sizeof(cell_params)))
		return -EFAULT;

	config = kmalloc(cell_params.config_size, GFP_KERNEL | GFP_DMA);
	if (!config)
		return -ENOMEM;

	if (copy_from_user(config,
			   (void *)(unsigned long)cell_params.config_address,
			   cell_params.config_size)) {
		err = -EFAULT;
		goto kfree_config_out;
	}
	config->name[JAILHOUSE_CELL_NAME_MAXLEN] = 0;

	if (mutex_lock_interruptible(&lock) != 0) {
		err = -EINTR;
		goto kfree_config_out;
	}

	if (!enabled) {
		err = -EINVAL;
		goto unlock_out;
	}

	cell_id.id = JAILHOUSE_CELL_ID_UNUSED;
	memcpy(cell_id.name, config->name, sizeof(cell_id.name));
	if (find_cell(&cell_id) != NULL) {
		err = -EEXIST;
		goto unlock_out;
	}

	cell = create_cell(config);
	if (IS_ERR(cell)) {
		err = PTR_ERR(cell);
		goto unlock_out;
	}

	if (!cpumask_subset(&cell->cpus_assigned, &root_cell->cpus_assigned)) {
		err = -EBUSY;
		goto error_cell_delete;
	}

	for_each_cpu(cpu, &cell->cpus_assigned) {
		if (cpu_online(cpu)) {
			err = cpu_down(cpu);
			if (err)
				goto error_cpu_online;
			cpu_set(cpu, offlined_cpus);
		}
		cpu_clear(cpu, root_cell->cpus_assigned);
	}

	id = jailhouse_call_arg1(JAILHOUSE_HC_CELL_CREATE, __pa(config));
	if (id < 0) {
		err = id;
		goto error_cpu_online;
	}

	cell->id = id;
	register_cell(cell);

	pr_info("Created Jailhouse cell \"%s\"\n", config->name);

unlock_out:
	mutex_unlock(&lock);

kfree_config_out:
	kfree(config);

	return err;

error_cpu_online:
	for_each_cpu(cpu, &cell->cpus_assigned) {
		if (!cpu_online(cpu) && cpu_up(cpu) == 0)
			cpu_clear(cpu, offlined_cpus);
		cpu_set(cpu, root_cell->cpus_assigned);
	}

error_cell_delete:
	delete_cell(cell);
	goto unlock_out;
}

static int cell_management_prologue(struct jailhouse_cell_id *cell_id,
				    struct cell **cell_ptr)
{
	cell_id->name[JAILHOUSE_CELL_ID_NAMELEN] = 0;

	if (mutex_lock_interruptible(&lock) != 0)
		return -EINTR;

	if (!enabled) {
		mutex_unlock(&lock);
		return -EINVAL;
	}

	*cell_ptr = find_cell(cell_id);
	if (*cell_ptr == NULL) {
		mutex_unlock(&lock);
		return -ENOENT;
	}
	return 0;
}

#define MEM_REQ_FLAGS	(JAILHOUSE_MEM_WRITE | JAILHOUSE_MEM_LOADABLE)

static int load_image(struct cell *cell,
		      struct jailhouse_preload_image __user *uimage)
{
	struct jailhouse_preload_image image;
	const struct jailhouse_memory *mem;
	unsigned int regions;
	u64 image_offset;
	void *image_mem;
	int err = 0;

	if (copy_from_user(&image, uimage, sizeof(image)))
		return -EFAULT;

	mem = cell->memory_regions;
	for (regions = cell->num_memory_regions; regions > 0; regions--) {
		image_offset = image.target_address - mem->virt_start;
		if (image.target_address >= mem->virt_start &&
		    image_offset < mem->size) {
			if (image.size > mem->size - image_offset ||
			    (mem->flags & MEM_REQ_FLAGS) != MEM_REQ_FLAGS)
				return -EINVAL;
			break;
		}
		mem++;
	}
	if (regions == 0)
		return -EINVAL;

	image_mem = jailhouse_ioremap(mem->phys_start + image_offset, 0,
				      image.size);
	if (!image_mem) {
		pr_err("jailhouse: Unable to map cell RAM at %08llx "
		       "for image loading\n",
		       (unsigned long long)(mem->phys_start + image_offset));
		return -EBUSY;
	}

	if (copy_from_user(image_mem,
			   (void *)(unsigned long)image.source_address,
			   image.size))
		err = -EFAULT;

	vunmap(image_mem);

	return err;
}

static int jailhouse_cell_load(struct jailhouse_cell_load __user *arg)
{
	struct jailhouse_preload_image __user *image = arg->image;
	struct jailhouse_cell_load cell_load;
	struct cell *cell;
	unsigned int n;
	int err;

	if (copy_from_user(&cell_load, arg, sizeof(cell_load)))
		return -EFAULT;

	err = cell_management_prologue(&cell_load.cell_id, &cell);
	if (err)
		return err;

	err = jailhouse_call_arg1(JAILHOUSE_HC_CELL_SET_LOADABLE, cell->id);
	if (err)
		goto unlock_out;

	for (n = cell_load.num_preload_images; n > 0; n--, image++) {
		err = load_image(cell, image);
		if (err)
			break;
	}

unlock_out:
	mutex_unlock(&lock);

	return err;
}

static int jailhouse_cell_start(const char __user *arg)
{
	struct jailhouse_cell_id cell_id;
	struct cell *cell;
	int err;

	if (copy_from_user(&cell_id, arg, sizeof(cell_id)))
		return -EFAULT;

	err = cell_management_prologue(&cell_id, &cell);
	if (err)
		return err;

	err = jailhouse_call_arg1(JAILHOUSE_HC_CELL_START, cell->id);

	mutex_unlock(&lock);

	return err;
}

static int jailhouse_cell_destroy(const char __user *arg)
{
	struct jailhouse_cell_id cell_id;
	struct cell *cell;
	unsigned int cpu;
	int err;

	if (copy_from_user(&cell_id, arg, sizeof(cell_id)))
		return -EFAULT;

	err = cell_management_prologue(&cell_id, &cell);
	if (err)
		return err;

	err = jailhouse_call_arg1(JAILHOUSE_HC_CELL_DESTROY, cell->id);
	if (err)
		goto unlock_out;

	for_each_cpu(cpu, &cell->cpus_assigned) {
		if (cpu_isset(cpu, offlined_cpus)) {
			if (cpu_up(cpu) != 0)
				pr_err("Jailhouse: failed to bring CPU %d "
				       "back online\n", cpu);
			cpu_clear(cpu, offlined_cpus);
		}
		cpu_set(cpu, root_cell->cpus_assigned);
	}

	pr_info("Destroyed Jailhouse cell \"%s\"\n",
		kobject_name(&cell->kobj));

	delete_cell(cell);

unlock_out:
	mutex_unlock(&lock);

	return err;
}

static long jailhouse_ioctl(struct file *file, unsigned int ioctl,
			    unsigned long arg)
{
	long err;

	switch (ioctl) {
	case JAILHOUSE_ENABLE:
		err = jailhouse_enable(
			(struct jailhouse_system __user *)arg);
		break;
	case JAILHOUSE_DISABLE:
		err = jailhouse_disable();
		break;
	case JAILHOUSE_CELL_CREATE:
		err = jailhouse_cell_create(
			(struct jailhouse_cell_create __user *)arg);
		break;
	case JAILHOUSE_CELL_LOAD:
		err = jailhouse_cell_load(
			(struct jailhouse_cell_load __user *)arg);
		break;
	case JAILHOUSE_CELL_START:
		err = jailhouse_cell_start((const char __user *)arg);
		break;
	case JAILHOUSE_CELL_DESTROY:
		err = jailhouse_cell_destroy((const char __user *)arg);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static const struct file_operations jailhouse_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = jailhouse_ioctl,
	.compat_ioctl = jailhouse_ioctl,
	.llseek = noop_llseek,
};

static struct miscdevice jailhouse_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "jailhouse",
	.fops = &jailhouse_fops,
};

static int jailhouse_shutdown_notify(struct notifier_block *unused1,
				     unsigned long unused2, void *unused3)
{
	int err;

	err = jailhouse_disable();
	if (err && err != -EINVAL)
		pr_emerg("jailhouse: ordered shutdown failed!\n");

	return NOTIFY_DONE;
}

static struct notifier_block jailhouse_shutdown_nb = {
	.notifier_call = jailhouse_shutdown_notify,
};

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
			    char *buffer)
{
	return sprintf(buffer, "%d\n", enabled);
}

static ssize_t info_show(struct device *dev, char *buffer, unsigned int type)
{
	ssize_t result;
	long val = 0;

	if (mutex_lock_interruptible(&lock) != 0)
		return -EINTR;

	if (enabled)
		val = jailhouse_call_arg1(JAILHOUSE_HC_HYPERVISOR_GET_INFO,
					  type);
	if (val >= 0)
		result = sprintf(buffer, "%ld\n", val);
	else
		result = val;

	mutex_unlock(&lock);
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

static DEVICE_ATTR_RO(enabled);
static DEVICE_ATTR_RO(mem_pool_size);
static DEVICE_ATTR_RO(mem_pool_used);
static DEVICE_ATTR_RO(remap_pool_size);
static DEVICE_ATTR_RO(remap_pool_used);

static struct attribute *jailhouse_sysfs_entries[] = {
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

static int __init jailhouse_init(void)
{
	int err;

	jailhouse_dev = root_device_register("jailhouse");
	if (IS_ERR(jailhouse_dev))
		return PTR_ERR(jailhouse_dev);

	err = sysfs_create_group(&jailhouse_dev->kobj,
				 &jailhouse_attribute_group);
	if (err)
		goto unreg_dev;

	cells_dir = kobject_create_and_add("cells", &jailhouse_dev->kobj);
	if (!cells_dir) {
		err = -ENOMEM;
		goto remove_attrs;
	}

	err = misc_register(&jailhouse_misc_dev);
	if (err)
		goto remove_cells_dir;

	register_reboot_notifier(&jailhouse_shutdown_nb);

	init_hypercall();

	return 0;

remove_cells_dir:
	kobject_put(cells_dir);

remove_attrs:
	sysfs_remove_group(&jailhouse_dev->kobj, &jailhouse_attribute_group);

unreg_dev:
	root_device_unregister(jailhouse_dev);
	return err;
}

static void __exit jailhouse_exit(void)
{
	unregister_reboot_notifier(&jailhouse_shutdown_nb);
	misc_deregister(&jailhouse_misc_dev);
	kobject_put(cells_dir);
	sysfs_remove_group(&jailhouse_dev->kobj, &jailhouse_attribute_group);
	root_device_unregister(jailhouse_dev);
}

module_init(jailhouse_init);
module_exit(jailhouse_exit);
