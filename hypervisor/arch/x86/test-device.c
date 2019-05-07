/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2018
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/printk.h>
#include <jailhouse/unit.h>

static unsigned int testdev_mmio_count_regions(struct cell *cell)
{
	return cell->config->flags & JAILHOUSE_CELL_TEST_DEVICE ? 1 : 0;
}

static enum mmio_result testdev_handle_mmio_access(void *arg,
						   struct mmio_access *mmio)
{
	void *test_reg = &this_cell()->comm_page.padding[mmio->address];

	if (mmio->address < 0xff8 || mmio->address > 0x1000 - mmio->size)
		goto invalid_access;

	switch (mmio->size) {
	case 1:
		if (mmio->is_write)
			*(u8 *)test_reg = mmio->value;
		else
			mmio->value = *(u8 *)test_reg;
		break;
	case 2:
		if (mmio->is_write)
			*(u16 *)test_reg = mmio->value;
		else
			mmio->value = *(u16 *)test_reg;
		break;
	case 4:
		if (mmio->is_write)
			*(u32 *)test_reg = mmio->value;
		else
			mmio->value = *(u32 *)test_reg;
		break;
	case 8:
		if (mmio->is_write)
			*(u64 *)test_reg = mmio->value;
		else
			mmio->value = *(u64 *)test_reg;
		break;
	}
	return MMIO_HANDLED;

invalid_access:
	printk("testdev: invalid %s, register %lx, size %d\n",
	       mmio->is_write ? "write" : "read", mmio->address, mmio->size);
	return MMIO_ERROR;
}

static unsigned long testdev_get_mmio_base(struct cell *cell)
{
	const struct jailhouse_memory *mem;
	unsigned int n;

	/* The mmio test page is one page after the COMM_REGION */
	for_each_mem_region(mem, cell->config, n)
		if (mem->flags & JAILHOUSE_MEM_COMM_REGION)
			return mem->virt_start + PAGE_SIZE;

	return INVALID_PHYS_ADDR;
}

static int testdev_cell_init(struct cell *cell)
{
	unsigned long mmio_base;

	if (cell->config->flags & JAILHOUSE_CELL_TEST_DEVICE) {
		mmio_base = testdev_get_mmio_base(cell);
		if (mmio_base == INVALID_PHYS_ADDR)
			return trace_error(-EINVAL);

		mmio_region_register(cell, mmio_base, PAGE_SIZE,
				     testdev_handle_mmio_access, NULL);
	}
	return 0;
}

static void testdev_cell_exit(struct cell *cell)
{
	if (cell->config->flags & JAILHOUSE_CELL_TEST_DEVICE)
		mmio_region_unregister(cell, testdev_get_mmio_base(cell));
}

static int testdev_init(void)
{
	return 0;
}

DEFINE_UNIT_SHUTDOWN_STUB(testdev);
DEFINE_UNIT(testdev, "Test device");
