/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2014-2016
 *
 * Authors:
 *  Henning Schild <henning.schild@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */
#include <inmate.h>

#define VENDORID	0x1af4
#define DEVICEID	0x1110

#define IVSHMEM_CFG_SHMEM_PTR	0x40
#define IVSHMEM_CFG_SHMEM_SZ	0x48

#define JAILHOUSE_SHMEM_PROTO_UNDEFINED	0x0000

#define IRQ_VECTOR	32

#define MAX_NDEV	4
#define UART_BASE	0x3F8

static char str[32] = "Hello From IVSHMEM  ";
static int ndevices;
static int irq_counter;

struct ivshmem_dev_data {
	u16 bdf;
	u32 *registers;
	void *shmem;
	u32 *msix_table;
	u64 shmemsz;
	u64 bar2sz;
};

static struct ivshmem_dev_data devs[MAX_NDEV];

static u64 pci_cfg_read64(u16 bdf, unsigned int addr)
{
	u64 bar;

	bar = ((u64)pci_read_config(bdf, addr + 4, 4) << 32) |
	      pci_read_config(bdf, addr, 4);
	return bar;
}

static void pci_cfg_write64(u16 bdf, unsigned int addr, u64 val)
{
	pci_write_config(bdf, addr + 4, (u32)(val >> 32), 4);
	pci_write_config(bdf, addr, (u32)val, 4);
}

static u64 get_bar_sz(u16 bdf, u8 barn)
{
	u64 bar, tmp;
	u64 barsz;

	bar = pci_cfg_read64(bdf, PCI_CFG_BAR + (8 * barn));
	pci_cfg_write64(bdf, PCI_CFG_BAR + (8 * barn), 0xffffffffffffffffULL);
	tmp = pci_cfg_read64(bdf, PCI_CFG_BAR + (8 * barn));
	barsz = ~(tmp & ~(0xf)) + 1;
	pci_cfg_write64(bdf, PCI_CFG_BAR + (8 * barn), bar);

	return barsz;
}

static void map_shmem_and_bars(struct ivshmem_dev_data *d)
{
	int cap = pci_find_cap(d->bdf, PCI_CAP_MSIX);

	if (cap < 0) {
		printk("IVSHMEM ERROR: device is not MSI-X capable\n");
		return;
	}

	d->shmemsz = pci_cfg_read64(d->bdf, IVSHMEM_CFG_SHMEM_SZ);
	d->shmem = (void *)pci_cfg_read64(d->bdf, IVSHMEM_CFG_SHMEM_PTR);

	printk("IVSHMEM: shmem is at %p\n", d->shmem);
	d->registers = (u32 *)((u64)(d->shmem + d->shmemsz + PAGE_SIZE - 1)
		& PAGE_MASK);
	pci_cfg_write64(d->bdf, PCI_CFG_BAR, (u64)d->registers);
	printk("IVSHMEM: bar0 is at %p\n", d->registers);
	d->bar2sz = get_bar_sz(d->bdf, 2);
	d->msix_table = (u32 *)((u64)d->registers + PAGE_SIZE);
	pci_cfg_write64(d->bdf, PCI_CFG_BAR + 16, (u64)d->msix_table);
	printk("IVSHMEM: bar2 is at %p\n", d->msix_table);

	pci_write_config(d->bdf, PCI_CFG_COMMAND,
			 (PCI_CMD_MEM | PCI_CMD_MASTER), 2);
	map_range(d->shmem, d->shmemsz + PAGE_SIZE + d->bar2sz, MAP_UNCACHED);
}

static int get_ivpos(struct ivshmem_dev_data *d)
{
	return mmio_read32(d->registers + 2);
}

static void send_irq(struct ivshmem_dev_data *d)
{
	printk("IVSHMEM: %02x:%02x.%x sending IRQ\n",
	       d->bdf >> 8, (d->bdf >> 3) & 0x1f, d->bdf & 0x3);
	mmio_write32(d->registers + 3, 1);
}

static void irq_handler(void)
{
	printk("IVSHMEM: got interrupt ... %d\n", irq_counter++);
}

void inmate_main(void)
{
	int i;
	int bdf = 0;
	unsigned int class_rev;
	struct ivshmem_dev_data *d;
	volatile char *shmem;

	int_init();

	while ((ndevices < MAX_NDEV) &&
	       (-1 != (bdf = pci_find_device(VENDORID, DEVICEID, bdf)))) {
		printk("IVSHMEM: Found %04x:%04x at %02x:%02x.%x\n",
		       pci_read_config(bdf, PCI_CFG_VENDOR_ID, 2),
		       pci_read_config(bdf, PCI_CFG_DEVICE_ID, 2),
		       bdf >> 8, (bdf >> 3) & 0x1f, bdf & 0x3);
		class_rev = pci_read_config(bdf, 0x8, 4);
		if (class_rev != (PCI_DEV_CLASS_OTHER << 24 |
				  JAILHOUSE_SHMEM_PROTO_UNDEFINED << 8)) {
			printk("IVSHMEM: class/revision %08x, not supported "
			       "skipping device\n", class_rev);
			bdf++;
			continue;
		}
		ndevices++;
		d = devs + ndevices - 1;
		d->bdf = bdf;
		map_shmem_and_bars(d);
		printk("IVSHMEM: mapped the bars got position %d\n",
			get_ivpos(d));

		memcpy(d->shmem, str, 32);

		int_set_handler(IRQ_VECTOR + ndevices - 1, irq_handler);
		pci_msix_set_vector(bdf, IRQ_VECTOR + ndevices - 1, 0);
		bdf++;
	}

	if (!ndevices) {
		printk("IVSHMEM: No PCI devices found .. nothing to do.\n");
		goto out;
	}

	asm volatile("sti");
	while (1) {
		for (i = 0; i < ndevices; i++) {
			d = devs + i;
			delay_us(1000*1000);
			shmem = d->shmem;
			shmem[19]++;
			send_irq(d);
		}
	}
out:
	asm volatile("hlt");
}
