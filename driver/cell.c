/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2015
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <linux/cpu.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>

#include "cell.h"
#include "main.h"
#include "pci.h"
#include "sysfs.h"

#include <jailhouse/hypercall.h>

struct cell *root_cell;

static LIST_HEAD(cells);
static cpumask_t offlined_cpus;

void jailhouse_cell_kobj_release(struct kobject *kobj)
{
	struct cell *cell = container_of(kobj, struct cell, kobj);

	jailhouse_pci_cell_cleanup(cell);
	vfree(cell->memory_regions);
	kfree(cell);
}

struct cell *jailhouse_cell_create(const struct jailhouse_cell_desc *cell_desc)
{
	struct cell *cell;
	int err;

	if (cell_desc->num_memory_regions >=
	    ULONG_MAX / sizeof(struct jailhouse_memory))
		return ERR_PTR(-EINVAL);

	cell = kzalloc(sizeof(*cell), GFP_KERNEL);
	if (!cell)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&cell->entry);

	bitmap_copy(cpumask_bits(&cell->cpus_assigned),
		    jailhouse_cell_cpu_set(cell_desc),
		    min(nr_cpumask_bits, (int)cell_desc->cpu_set_size * 8));

	cell->num_memory_regions = cell_desc->num_memory_regions;
	cell->memory_regions = vmalloc(sizeof(struct jailhouse_memory) *
				       cell->num_memory_regions);
	if (!cell->memory_regions) {
		kfree(cell);
		return ERR_PTR(-ENOMEM);
	}

	memcpy(cell->memory_regions, jailhouse_cell_mem_regions(cell_desc),
	       sizeof(struct jailhouse_memory) * cell->num_memory_regions);

	err = jailhouse_pci_cell_setup(cell, cell_desc);
	if (err) {
		vfree(cell->memory_regions);
		kfree(cell);
		return ERR_PTR(err);
	}

	err = jailhouse_sysfs_cell_create(cell, cell_desc->name);
	if (err)
		/* cleanup done by jailhouse_sysfs_cell_create */
		return ERR_PTR(err);

	return cell;
}

void jailhouse_cell_register(struct cell *cell)
{
	list_add_tail(&cell->entry, &cells);
	jailhouse_sysfs_cell_register(cell);
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

void jailhouse_cell_delete(struct cell *cell)
{
	list_del(&cell->entry);
	jailhouse_sysfs_cell_delete(cell);
}

int jailhouse_cell_prepare_root(const struct jailhouse_cell_desc *cell_desc)
{
	root_cell = jailhouse_cell_create(cell_desc);
	if (IS_ERR(root_cell))
		return PTR_ERR(root_cell);

	cpumask_and(&root_cell->cpus_assigned, &root_cell->cpus_assigned,
		    cpu_online_mask);

	return 0;
}

void jailhouse_cell_register_root(void)
{
	jailhouse_pci_do_all_devices(root_cell, JAILHOUSE_PCI_TYPE_IVSHMEM,
				     JAILHOUSE_PCI_ACTION_ADD);

	root_cell->id = 0;
	jailhouse_cell_register(root_cell);
}

void jailhouse_cell_delete_root(void)
{
	jailhouse_cell_delete(root_cell);
}

void jailhouse_cell_delete_all(void)
{
	struct cell *cell, *tmp;
	unsigned int cpu;

	jailhouse_pci_do_all_devices(root_cell, JAILHOUSE_PCI_TYPE_IVSHMEM,
				     JAILHOUSE_PCI_ACTION_DEL);

	jailhouse_pci_do_all_devices(root_cell, JAILHOUSE_PCI_TYPE_DEVICE,
				     JAILHOUSE_PCI_ACTION_RELEASE);

	list_for_each_entry_safe(cell, tmp, &cells, entry)
		jailhouse_cell_delete(cell);

	for_each_cpu(cpu, &offlined_cpus) {
		if (cpu_up(cpu) != 0)
			pr_err("Jailhouse: failed to bring CPU %d back "
			       "online\n", cpu);
		cpumask_clear_cpu(cpu, &offlined_cpus);
	}
}

int jailhouse_cmd_cell_create(struct jailhouse_cell_create __user *arg)
{
	struct jailhouse_cell_create cell_params;
	struct jailhouse_cell_desc *config;
	struct jailhouse_cell_id cell_id;
	void __user *user_config;
	struct cell *cell;
	unsigned int cpu;
	int id, err = 0;

	if (copy_from_user(&cell_params, arg, sizeof(cell_params)))
		return -EFAULT;

	config = kmalloc(cell_params.config_size, GFP_USER | __GFP_NOWARN);
	if (!config)
		return -ENOMEM;

	user_config = (void __user *)(unsigned long)cell_params.config_address;
	if (copy_from_user(config, user_config, cell_params.config_size)) {
		err = -EFAULT;
		goto kfree_config_out;
	}

	if (memcmp(config->signature, JAILHOUSE_CELL_DESC_SIGNATURE,
		   sizeof(config->signature)) != 0) {
		pr_err("jailhouse: Not a cell configuration\n");
		err = -EINVAL;
		goto kfree_config_out;
	}

	config->name[JAILHOUSE_CELL_NAME_MAXLEN] = 0;

	if (mutex_lock_interruptible(&jailhouse_lock) != 0) {
		err = -EINTR;
		goto kfree_config_out;
	}

	if (!jailhouse_enabled) {
		err = -EINVAL;
		goto unlock_out;
	}

	cell_id.id = JAILHOUSE_CELL_ID_UNUSED;
	memcpy(cell_id.name, config->name, sizeof(cell_id.name));
	if (find_cell(&cell_id) != NULL) {
		err = -EEXIST;
		goto unlock_out;
	}

	cell = jailhouse_cell_create(config);
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
			cpumask_set_cpu(cpu, &offlined_cpus);
		}
		cpumask_clear_cpu(cpu, &root_cell->cpus_assigned);
	}

	jailhouse_pci_do_all_devices(cell, JAILHOUSE_PCI_TYPE_DEVICE,
	                             JAILHOUSE_PCI_ACTION_CLAIM);

	id = jailhouse_call_arg1(JAILHOUSE_HC_CELL_CREATE, __pa(config));
	if (id < 0) {
		err = id;
		goto error_cpu_online;
	}

	cell->id = id;
	jailhouse_cell_register(cell);

	pr_info("Created Jailhouse cell \"%s\"\n", config->name);

unlock_out:
	mutex_unlock(&jailhouse_lock);

kfree_config_out:
	kfree(config);

	return err;

error_cpu_online:
	for_each_cpu(cpu, &cell->cpus_assigned) {
		if (!cpu_online(cpu) && cpu_up(cpu) == 0)
			cpumask_clear_cpu(cpu, &offlined_cpus);
		cpumask_set_cpu(cpu, &root_cell->cpus_assigned);
	}

error_cell_delete:
	jailhouse_cell_delete(cell);
	goto unlock_out;
}

static int cell_management_prologue(struct jailhouse_cell_id *cell_id,
				    struct cell **cell_ptr)
{
	cell_id->name[JAILHOUSE_CELL_ID_NAMELEN] = 0;

	if (mutex_lock_interruptible(&jailhouse_lock) != 0)
		return -EINTR;

	if (!jailhouse_enabled) {
		mutex_unlock(&jailhouse_lock);
		return -EINVAL;
	}

	*cell_ptr = find_cell(cell_id);
	if (*cell_ptr == NULL) {
		mutex_unlock(&jailhouse_lock);
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
	unsigned int regions, page_offs;
	u64 image_offset, phys_start;
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

	phys_start = (mem->phys_start + image_offset) & PAGE_MASK;
	page_offs = offset_in_page(image_offset);
	image_mem = jailhouse_ioremap(phys_start, 0,
				      PAGE_ALIGN(image.size + page_offs));
	if (!image_mem) {
		pr_err("jailhouse: Unable to map cell RAM at %08llx "
		       "for image loading\n",
		       (unsigned long long)(mem->phys_start + image_offset));
		return -EBUSY;
	}

	if (copy_from_user(image_mem + page_offs,
			   (void __user *)(unsigned long)image.source_address,
			   image.size))
		err = -EFAULT;
	/*
	 * ARMv8 requires to clean D-cache and invalidate I-cache for memory
	 * containing new instructions. On x86 this is a NOP. On ARMv7 the
	 * firmware does its own cache maintenance, so it is an
	 * extraneous (but harmless) flush.
	 */
	flush_icache_range((unsigned long)(image_mem + page_offs),
			   (unsigned long)(image_mem + page_offs) + image.size);

	vunmap(image_mem);

	return err;
}

int jailhouse_cmd_cell_load(struct jailhouse_cell_load __user *arg)
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
	mutex_unlock(&jailhouse_lock);

	return err;
}

int jailhouse_cmd_cell_start(const char __user *arg)
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

	mutex_unlock(&jailhouse_lock);

	return err;
}

int jailhouse_cmd_cell_destroy(const char __user *arg)
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
		if (cpumask_test_cpu(cpu, &offlined_cpus)) {
			if (cpu_up(cpu) != 0)
				pr_err("Jailhouse: failed to bring CPU %d "
				       "back online\n", cpu);
			cpumask_clear_cpu(cpu, &offlined_cpus);
		}
		cpumask_set_cpu(cpu, &root_cell->cpus_assigned);
	}

	jailhouse_pci_do_all_devices(cell, JAILHOUSE_PCI_TYPE_DEVICE,
	                             JAILHOUSE_PCI_ACTION_RELEASE);

	pr_info("Destroyed Jailhouse cell \"%s\"\n",
		kobject_name(&cell->kobj));

	jailhouse_cell_delete(cell);

unlock_out:
	mutex_unlock(&jailhouse_lock);

	return err;
}
