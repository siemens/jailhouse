/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 *
 * Append "-device intel-hda,addr=1b.0 -device hda-output" to the QEMU command
 * line for testing in the virtual machine. Adjust configs/x86/pci-demo.c for
 * real machines as needed.
 */

#include <inmate.h>

#define IRQ_VECTOR		32

#define HDA_GCTL		0x08
#define HDA_WAKEEN		0x0c
#define HDA_STATESTS		0x0e
#define HDA_INTCTL		0x20

static void *hdbar;

static void irq_handler(unsigned int irq)
{
	u16 statests;

	if (irq != IRQ_VECTOR)
		return;

	statests = mmio_read16(hdbar + HDA_STATESTS);

	printk("HDA MSI received (STATESTS: %04x)\n", statests);
	mmio_write16(hdbar + HDA_STATESTS, statests);
}

void inmate_main(void)
{
	u64 bar;
	int bdf;

	irq_init(irq_handler);

	bdf = pci_find_device(PCI_ID_ANY, PCI_ID_ANY, 0);
	if (bdf < 0) {
		printk("No device found!\n");
		return;
	}
	printk("Found %04x:%04x at %02x:%02x.%x\n",
	       pci_read_config(bdf, PCI_CFG_VENDOR_ID, 2),
	       pci_read_config(bdf, PCI_CFG_DEVICE_ID, 2),
	       bdf >> 8, (bdf >> 3) & 0x1f, bdf & 0x3);

	bar = pci_read_config(bdf, PCI_CFG_BAR, 4);
	if ((bar & 0x6) == 0x4)
		bar |= (u64)pci_read_config(bdf, PCI_CFG_BAR + 4, 4) << 32;
	hdbar = (void *)(bar & ~0xfUL);
	map_range(hdbar, PAGE_SIZE, MAP_UNCACHED);
	printk("HDBAR at %p\n", hdbar);

	pci_msi_set_vector(bdf, IRQ_VECTOR);

	pci_write_config(bdf, PCI_CFG_COMMAND,
			 PCI_CMD_MEM | PCI_CMD_MASTER, 2);

	asm volatile("sti");

	mmio_write16(hdbar + HDA_STATESTS, mmio_read16(hdbar + HDA_STATESTS));

	mmio_write16(hdbar + HDA_GCTL, 0);
	delay_us(7000);
	mmio_write16(hdbar + HDA_GCTL, 1);

	mmio_write16(hdbar + HDA_WAKEEN, 0x0f);
	mmio_write32(hdbar + HDA_INTCTL, (1 << 31) | (1 << 30));

	halt();
}
