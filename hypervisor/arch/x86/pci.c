/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Ivan Kolchin <ivan.kolchin@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/pci.h>
#include <jailhouse/utils.h>
#include <asm/io.h>
#include <asm/pci.h>

/* protects the root bridge's PIO interface to the PCI config space */
static DEFINE_SPINLOCK(pci_lock);

/**
 * set_rax_reg() - Set value of RAX in guest register set
 * @guest_regs:	Guest register set
 * @value_new:	New value to be written
 * @size:	Access size (1, 2 or 4 bytes)
 */
static void set_rax_reg(struct registers *guest_regs,
	u32 value_new, u8 size)
{
	u64 value_old = guest_regs->rax;
	/* 32-bit access is special, since it clears all the upper
	 *  part of RAX. Another types of access leave it intact */
	u64 mask = (size == 4 ? BYTE_MASK(8) : BYTE_MASK(size));

	guest_regs->rax = (value_old & ~mask) | (value_new & mask);
}

/**
 * get_rax_reg() - Get value of RAX from guest register set
 * @guest_regs:	Guest register set
 * @size:	Access size (1, 2 or 4 bytes)
 *
 * Return: Register value
 */
static u32 get_rax_reg(struct registers *guest_regs, u8 size)
{
	return guest_regs->rax & BYTE_MASK(size);
}

/**
 * data_port_in_handler() - Handler for IN accesses to data port
 * @guest_regs:	Guest register set
 * @port:	I/O port number
 * @size:	Access size (1, 2 or 4 bytes)
 * @device:	Structure describing PCI device
 */
static int
data_port_in_handler(struct registers *guest_regs, u16 port, unsigned int size,
		     const struct jailhouse_pci_device *device)
{
	u32 reg_data;

	if (!device)
		reg_data = -1;
	else
		reg_data = inl(PCI_REG_DATA_PORT);
	reg_data >>= (port - PCI_REG_DATA_PORT) * 8;

	set_rax_reg(guest_regs, reg_data, size);

	return 1;
}

/**
 * data_port_out_handler() - Handler for OUT accesses to data port
 * @guest_regs:	Guest register set
 * @port:	I/O port number
 * @size:	Access size (1, 2 or 4 bytes)
 * @device:	Structure describing PCI device
 * @reg_num:	Register number in PCI configuration space
 */
static int data_port_out_handler(struct registers *guest_regs, u16 port,
				 unsigned int size,
				 const struct jailhouse_pci_device *device,
				 u8 reg_num)
{
	u32 reg_data;

	if (!device)
		return 1;

	reg_data = get_rax_reg(guest_regs, size);

	if (reg_num < PCI_CONFIG_HEADER_SIZE) {
		if (pci_cfg_write_allowed(device->type, reg_num,
					  port - PCI_REG_DATA_PORT, size)) {
			if (size == 1)
				outb(reg_data, port);
			else if (size == 2)
				outw(reg_data, port);
			else if (size == 4)
				outl(reg_data, port);
			return 1;
		}
	}

	return -1;
}

/**
 * x86_pci_config_handler() - Handler for accesses to PCI config space
 * @guest_regs:		Guest registers
 * @cell:		Issuing cell
 * @port:		I/O port number
 * @dir_in:		True for input, false for output
 * @size:		Size of access in bytes (1, 2 or 4 bytes)
 *
 * Return: 1 if handled successfully, 0 if unhandled, -1 on access error
 */
int x86_pci_config_handler(struct registers *guest_regs, struct cell *cell,
			   u16 port, bool dir_in, unsigned int size)
{
	const struct jailhouse_pci_device *device = NULL;
	u32 addr_port_val;
	int result = 0;
	u8 reg_num;
	u16 bdf;

	if (port == PCI_REG_ADDR_PORT) {
		/* only 4-byte accesses are valid */
		if (size != 4)
			return -1;

		if (dir_in)
			set_rax_reg(guest_regs, cell->pci_addr_port_val, size);
		else
			cell->pci_addr_port_val =
				get_rax_reg(guest_regs, size);
		result = 1;
	} else if (port >= PCI_REG_DATA_PORT &&
		   port < (PCI_REG_DATA_PORT + 4)) {
		/* overflowing accesses are invalid */
		if (port + size > PCI_REG_DATA_PORT + 4)
			return -1;

		/*
		 * Decode which register in PCI config space is accessed. It is
		 * essential to store the address port value locally so that we
		 * are not affected by concurrent manipulations by other CPUs
		 * of this cell.
		 */
		addr_port_val = cell->pci_addr_port_val;
		reg_num = addr_port_val & PCI_ADDR_REGNUM_MASK;

		/* get device (NULL if it doesn't belong to the cell) */
		bdf = addr_port_val >> PCI_ADDR_BDF_SHIFT;
		device = pci_get_assigned_device(cell, bdf);

		spin_lock(&pci_lock);

		outl(addr_port_val & PCI_ADDR_VALID_MASK, PCI_REG_ADDR_PORT);

		if (dir_in)
			result = data_port_in_handler(guest_regs, port, size,
						      device);
		else
			result = data_port_out_handler(guest_regs, port, size,
						       device, reg_num);

		spin_unlock(&pci_lock);
	}

	return result;
}
