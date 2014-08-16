/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 * Copyright (c) Valentine Sinitsyn, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/types.h>
#include <asm/i8042.h>
#include <asm/pci.h>
#include <asm/percpu.h>
#include <asm/vcpu.h>

/* Can be overriden in vendor-specific code if needed */
const u8 *vcpu_get_inst_bytes(const struct guest_paging_structures *pg_structs,
			      unsigned long pc, unsigned int *size)
	__attribute__((weak, alias("vcpu_map_inst")));

const u8 *vcpu_map_inst(const struct guest_paging_structures *pg_structs,
			unsigned long pc, unsigned int *size)
{
	unsigned short bytes_avail;
	u8 *page = NULL;

	if (!*size)
		goto out_err;
	page = paging_get_guest_pages(pg_structs, pc,
			1, PAGE_READONLY_FLAGS);
	if (!page)
		goto out_err;

	/* Number of bytes available before page boundary */
	bytes_avail = PAGE_SIZE - (pc & PAGE_OFFS_MASK);
	if (*size > bytes_avail)
		*size = bytes_avail;

	return &page[pc & PAGE_OFFS_MASK];

out_err:
	return NULL;
}

int vcpu_cell_init(struct cell *cell)
{
	const u8 *pio_bitmap = jailhouse_cell_pio_bitmap(cell->config);
	u32 pio_bitmap_size = cell->config->pio_bitmap_size;
	struct vcpu_io_bitmap cell_iobm, root_cell_iobm;
	unsigned int n, pm_timer_addr;
	u32 size;
	int err;
	u8 *b;

	/* PM timer has to be provided */
	if (system_config->platform_info.x86.pm_timer_address == 0)
		return -EINVAL;

	err = vcpu_vendor_cell_init(cell);
	if (err) {
		vcpu_cell_exit(cell);
		return err;
	}

	vcpu_vendor_get_cell_io_bitmap(cell, &cell_iobm);
	memset(cell_iobm.data, -1, cell_iobm.size);

	for (n = 0; n < 2; n++) {
		size = pio_bitmap_size <= PAGE_SIZE ?
			pio_bitmap_size : PAGE_SIZE;
		memcpy(cell_iobm.data + n * PAGE_SIZE, pio_bitmap, size);
		pio_bitmap += size;
		pio_bitmap_size -= size;
	}

	/* moderation access to i8042 command register */
	cell_iobm.data[I8042_CMD_REG / 8] |= 1 << (I8042_CMD_REG % 8);

	if (cell != &root_cell) {
		/*
		 * Shrink PIO access of root cell corresponding to new cell's
		 * access rights.
		 */
		vcpu_vendor_get_cell_io_bitmap(&root_cell, &root_cell_iobm);
		pio_bitmap = jailhouse_cell_pio_bitmap(cell->config);
		pio_bitmap_size = cell->config->pio_bitmap_size;
		for (b = root_cell_iobm.data; pio_bitmap_size > 0;
		     b++, pio_bitmap++, pio_bitmap_size--)
			*b |= ~*pio_bitmap;
	}

	/* permit access to the PM timer */
	pm_timer_addr = system_config->platform_info.x86.pm_timer_address;
	for (n = 0; n < 4; n++, pm_timer_addr++) {
		b = cell_iobm.data;
		b[pm_timer_addr / 8] &= ~(1 << (pm_timer_addr % 8));
	}

	return 0;
}

void vcpu_cell_exit(struct cell *cell)
{
	const u8 *root_pio_bitmap =
		jailhouse_cell_pio_bitmap(root_cell.config);
	const u8 *pio_bitmap = jailhouse_cell_pio_bitmap(cell->config);
	u32 pio_bitmap_size = cell->config->pio_bitmap_size;
	struct vcpu_io_bitmap root_cell_iobm;
	u8 *b;

	vcpu_vendor_get_cell_io_bitmap(&root_cell, &root_cell_iobm);

	if (root_cell.config->pio_bitmap_size < pio_bitmap_size)
		pio_bitmap_size = root_cell.config->pio_bitmap_size;

	for (b = root_cell_iobm.data; pio_bitmap_size > 0;
	     b++, pio_bitmap++, root_pio_bitmap++, pio_bitmap_size--)
		*b &= *pio_bitmap | *root_pio_bitmap;

	vcpu_vendor_cell_exit(cell);
}

void vcpu_handle_hypercall(struct registers *guest_regs,
			   struct vcpu_execution_state *x_state)
{
	bool long_mode = !!(x_state->efer & EFER_LMA);
	unsigned long arg_mask = long_mode ? (u64)-1 : (u32)-1;
	struct per_cpu *cpu_data = this_cpu_data();
	unsigned long code = guest_regs->rax;

	vcpu_skip_emulated_instruction(X86_INST_LEN_VMCALL);

	if ((!long_mode && (x_state->rflags & X86_RFLAGS_VM)) ||
	    (x_state->cs & 3) != 0) {
		guest_regs->rax = -EPERM;
		return;
	}

	guest_regs->rax = hypercall(code, guest_regs->rdi & arg_mask,
				    guest_regs->rsi & arg_mask);
	if (guest_regs->rax == -ENOSYS)
		printk("CPU %d: Unknown vmcall %d, RIP: %p\n",
		       cpu_data->cpu_id, code,
		       x_state->rip - X86_INST_LEN_VMCALL);

	if (code == JAILHOUSE_HC_DISABLE && guest_regs->rax == 0)
		vcpu_deactivate_vmm(guest_regs);
}

bool vcpu_handle_io_access(struct registers *guest_regs,
			   struct vcpu_io_intercept *io)
{
	struct per_cpu *cpu_data = this_cpu_data();
	int result = 0;

	/* string and REP-prefixed instructions are not supported */
	if (io->rep_or_str)
		goto invalid_access;

	result = x86_pci_config_handler(guest_regs, cpu_data->cell, io->port,
					io->in, io->size);
	if (result == 0)
		result = i8042_access_handler(guest_regs, io->port,
				              io->in, io->size);

	if (result == 1) {
		vcpu_skip_emulated_instruction(io->inst_len);
		return true;
	}

invalid_access:
	panic_printk("FATAL: Invalid PIO %s, port: %x size: %d\n",
		     io->in ? "read" : "write", io->port, io->size);
	panic_printk("PCI address port: %x\n",
		     cpu_data->cell->pci_addr_port_val);
	return false;
}
