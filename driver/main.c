/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2015
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
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/firmware.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <asm/smp.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include "cell.h"
#include "jailhouse.h"
#include "main.h"
#include "pci.h"
#include "sysfs.h"

#include <jailhouse/header.h>
#include <jailhouse/hypercall.h>
#include <generated/version.h>

#ifdef CONFIG_X86_32
#error 64-bit kernel required!
#endif

#if JAILHOUSE_CELL_ID_NAMELEN != JAILHOUSE_CELL_NAME_MAXLEN
# warning JAILHOUSE_CELL_ID_NAMELEN and JAILHOUSE_CELL_NAME_MAXLEN out of sync!
#endif

/* For compatibility with older kernel versions */
#include <linux/version.h>

#ifdef CONFIG_X86
#define JAILHOUSE_AMD_FW_NAME	"jailhouse-amd.bin"
#define JAILHOUSE_INTEL_FW_NAME	"jailhouse-intel.bin"
#else
#define JAILHOUSE_FW_NAME	"jailhouse.bin"
#endif

MODULE_DESCRIPTION("Management driver for Jailhouse partitioning hypervisor");
MODULE_LICENSE("GPL");
#ifdef CONFIG_X86
MODULE_FIRMWARE(JAILHOUSE_AMD_FW_NAME);
MODULE_FIRMWARE(JAILHOUSE_INTEL_FW_NAME);
#else
MODULE_FIRMWARE(JAILHOUSE_FW_NAME);
#endif
MODULE_VERSION(JAILHOUSE_VERSION);

DEFINE_MUTEX(jailhouse_lock);
bool jailhouse_enabled;

static struct device *jailhouse_dev;
static void *hypervisor_mem;
static unsigned long hv_core_and_percpu_size;
static atomic_t call_done;
static int error_code;

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

static long get_max_cpus(u32 cpu_set_size,
			 const struct jailhouse_system __user *system_config)
{
	u8 __user *cpu_set =
		(u8 __user *)jailhouse_cell_cpu_set(
				(const struct jailhouse_cell_desc * __force)
				&system_config->root_cell);
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

void *jailhouse_ioremap(phys_addr_t phys, unsigned long virt,
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
	int (*entry)(unsigned int);
	int err;

	entry = header->entry + (unsigned long) hypervisor_mem;

	if (cpu < header->max_cpus)
		/* either returns 0 or the same error code across all CPUs */
		err = entry(cpu);
	else
		err = -EINVAL;

	if (err)
		error_code = err;

#if defined(CONFIG_X86) && LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
	/* on Intel, VMXE is now on - update the shadow */
	cr4_init_shadow();
#endif

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

static int jailhouse_cmd_enable(struct jailhouse_system __user *arg)
{
	const struct firmware *hypervisor;
	struct jailhouse_system config_header;
	struct jailhouse_system *config;
	struct jailhouse_memory *hv_mem = &config_header.hypervisor_memory;
	struct jailhouse_header *header;
	unsigned long remap_addr = 0;
	void __iomem *console = NULL;
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

	if (memcmp(config_header.signature, JAILHOUSE_SYSTEM_SIGNATURE,
		   sizeof(config_header.signature)) != 0) {
		pr_err("jailhouse: Not a system configuration\n");
		return -EINVAL;
	}

	config_header.root_cell.name[JAILHOUSE_CELL_NAME_MAXLEN] = 0;

	max_cpus = get_max_cpus(config_header.root_cell.cpu_set_size, arg);
	if (max_cpus < 0)
		return max_cpus;
	if (max_cpus > UINT_MAX)
		return -EINVAL;

	if (mutex_lock_interruptible(&jailhouse_lock) != 0)
		return -EINTR;

	err = -EBUSY;
	if (jailhouse_enabled || !try_module_get(THIS_MODULE))
		goto error_unlock;

	err = request_firmware(&hypervisor, fw_name, jailhouse_dev);
	if (err) {
		pr_err("jailhouse: Missing hypervisor image %s\n", fw_name);
		goto error_put_module;
	}

	header = (struct jailhouse_header *)hypervisor->data;

	err = -EINVAL;
	if (memcmp(header->signature, JAILHOUSE_SIGNATURE,
		   sizeof(header->signature)) != 0 ||
	    hypervisor->size >= hv_mem->size)
		goto error_release_fw;

	hv_core_and_percpu_size = header->core_size +
		max_cpus * header->percpu_size;
	config_size = jailhouse_system_config_size(&config_header);
	if (hv_core_and_percpu_size >= hv_mem->size ||
	    config_size >= hv_mem->size - hv_core_and_percpu_size)
		goto error_release_fw;

#ifdef JAILHOUSE_BORROW_ROOT_PT
	remap_addr = JAILHOUSE_BASE;
#endif
	hypervisor_mem = jailhouse_ioremap(hv_mem->phys_start, remap_addr,
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

	if (config->debug_console.flags & JAILHOUSE_MEM_IO) {
#ifdef JAILHOUSE_BORROW_ROOT_PT
		console = ioremap(config->debug_console.phys_start,
				  config->debug_console.size);
		if (!console) {
			err = -EINVAL;
			pr_err("jailhouse: Unable to map hypervisor debug "
			       "console at %08lx\n",
			       (unsigned long)config->debug_console.phys_start);
			goto error_unmap;
		}
		/* The hypervisor has no notion of address spaces, so we need
		 * to enforce conversion. */
		header->debug_console_base = (void * __force)console;
#else
		header->debug_console_base =
			(void * __force) config->debug_console.phys_start;
#endif
	}

	err = jailhouse_cell_prepare_root(&config->root_cell);
	if (err)
		goto error_unmap;

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

	if (console)
		iounmap(console);

	release_firmware(hypervisor);

	jailhouse_cell_register_root();

	jailhouse_enabled = true;

	mutex_unlock(&jailhouse_lock);

	pr_info("The Jailhouse is opening.\n");

	return 0;

error_free_cell:
	jailhouse_cell_delete_root();

error_unmap:
	vunmap(hypervisor_mem);
	if (console)
		iounmap(console);

error_release_fw:
	release_firmware(hypervisor);

error_put_module:
	module_put(THIS_MODULE);

error_unlock:
	mutex_unlock(&jailhouse_lock);
	return err;
}

static void leave_hypervisor(void *info)
{
	void *page;
	int err;

	/* Touch each hypervisor page we may need during the switch so that
	 * the active mm definitely contains all mappings. At least x86 does
	 * not support taking any faults while switching worlds. */
	for (page = hypervisor_mem;
	     page < hypervisor_mem + hv_core_and_percpu_size;
	     page += PAGE_SIZE)
		readl((void __iomem *)page);

	/* either returns 0 or the same error code across all CPUs */
	err = jailhouse_call(JAILHOUSE_HC_DISABLE);
	if (err)
		error_code = err;

#if defined(CONFIG_X86) && LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0)
	/* on Intel, VMXE is now off - update the shadow */
	cr4_init_shadow();
#endif

	atomic_inc(&call_done);
}

static int jailhouse_cmd_disable(void)
{
	int err;

	if (mutex_lock_interruptible(&jailhouse_lock) != 0)
		return -EINTR;

	if (!jailhouse_enabled) {
		err = -EINVAL;
		goto unlock_out;
	}

	error_code = 0;

	preempt_disable();

	if (num_online_cpus() != cpumask_weight(&root_cell->cpus_assigned)) {
		/*
		 * Not all assigned CPUs are currently online. If we disable
		 * now, we will loose the offlined ones.
		 */

		preempt_enable();

		err = -EBUSY;
		goto unlock_out;
	}

	atomic_set(&call_done, 0);
	on_each_cpu(leave_hypervisor, NULL, 0);
	while (atomic_read(&call_done) != num_online_cpus())
		cpu_relax();

	preempt_enable();

	err = error_code;
	if (err)
		goto unlock_out;

	vunmap(hypervisor_mem);

	jailhouse_cell_delete_all();
	jailhouse_enabled = false;
	module_put(THIS_MODULE);

	pr_info("The Jailhouse was closed.\n");

unlock_out:
	mutex_unlock(&jailhouse_lock);

	return err;
}

static long jailhouse_ioctl(struct file *file, unsigned int ioctl,
			    unsigned long arg)
{
	long err;

	switch (ioctl) {
	case JAILHOUSE_ENABLE:
		err = jailhouse_cmd_enable(
			(struct jailhouse_system __user *)arg);
		break;
	case JAILHOUSE_DISABLE:
		err = jailhouse_cmd_disable();
		break;
	case JAILHOUSE_CELL_CREATE:
		err = jailhouse_cmd_cell_create(
			(struct jailhouse_cell_create __user *)arg);
		break;
	case JAILHOUSE_CELL_LOAD:
		err = jailhouse_cmd_cell_load(
			(struct jailhouse_cell_load __user *)arg);
		break;
	case JAILHOUSE_CELL_START:
		err = jailhouse_cmd_cell_start((const char __user *)arg);
		break;
	case JAILHOUSE_CELL_DESTROY:
		err = jailhouse_cmd_cell_destroy((const char __user *)arg);
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

	err = jailhouse_cmd_disable();
	if (err && err != -EINVAL)
		pr_emerg("jailhouse: ordered shutdown failed!\n");

	return NOTIFY_DONE;
}

static struct notifier_block jailhouse_shutdown_nb = {
	.notifier_call = jailhouse_shutdown_notify,
};

static int __init jailhouse_init(void)
{
	int err;

	jailhouse_dev = root_device_register("jailhouse");
	if (IS_ERR(jailhouse_dev))
		return PTR_ERR(jailhouse_dev);

	err = jailhouse_sysfs_init(jailhouse_dev);
	if (err)
		goto unreg_dev;

	err = misc_register(&jailhouse_misc_dev);
	if (err)
		goto exit_sysfs;

	err = jailhouse_pci_register();
	if (err)
		goto exit_misc;

	register_reboot_notifier(&jailhouse_shutdown_nb);

	init_hypercall();

	return 0;
exit_misc:
	misc_deregister(&jailhouse_misc_dev);

exit_sysfs:
	jailhouse_sysfs_exit(jailhouse_dev);

unreg_dev:
	root_device_unregister(jailhouse_dev);
	return err;
}

static void __exit jailhouse_exit(void)
{
	unregister_reboot_notifier(&jailhouse_shutdown_nb);
	misc_deregister(&jailhouse_misc_dev);
	jailhouse_sysfs_exit(jailhouse_dev);
	jailhouse_pci_unregister();
	root_device_unregister(jailhouse_dev);
}

module_init(jailhouse_init);
module_exit(jailhouse_exit);
