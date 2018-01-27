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
 * Append "-device e1000,addr=19,netdev=..." to the QEMU command line for
 * testing in the virtual machine. Adjust configs/x86/e1000-demo.c for real
 * machines as needed.
 */

#include <inmate.h>

#define E1000_REG_CTRL		0x0000
# define E1000_CTRL_LRST	(1 << 3)
# define E1000_CTRL_SLU		(1 << 6)
# define E1000_CTRL_FRCSPD	(1 << 11)
# define E1000_CTRL_RST		(1 << 26)
#define E1000_REG_STATUS	0x0008
# define E1000_STATUS_LU	(1 << 1)
# define E1000_STATUS_SPEEDSHFT	6
# define E1000_STATUS_SPEED	(3 << E1000_STATUS_SPEEDSHFT)
#define E1000_REG_EERD		0x0014
# define E1000_EERD_START	(1 << 0)
# define E1000_EERD_DONE	(1 << 4)
# define E1000_EERD_ADDR_SHIFT	8
# define E1000_EERD_DATA_SHIFT	16
#define E1000_REG_MDIC		0x0020
# define E1000_MDIC_REGADD_SHFT	16
# define E1000_MDIC_PHYADD	(0x1 << 21)
# define E1000_MDIC_OP_WRITE	(0x1 << 26)
# define E1000_MDIC_OP_READ	(0x2 << 26)
# define E1000_MDIC_READY	(0x1 << 28)
#define E1000_REG_RCTL		0x0100
# define E1000_RCTL_EN		(1 << 1)
# define E1000_RCTL_BAM		(1 << 15)
# define E1000_RCTL_BSIZE_2048	(0 << 16)
# define E1000_RCTL_SECRC	(1 << 26)
#define E1000_REG_TCTL		0x0400
# define E1000_TCTL_EN		(1 << 1)
# define E1000_TCTL_PSP		(1 << 3)
# define E1000_TCTL_CT_DEF	(0xf << 4)
# define E1000_TCTL_COLD_DEF	(0x40 << 12)
#define E1000_REG_TIPG		0x0410
# define E1000_TIPG_IPGT_DEF	(10 << 0)
# define E1000_TIPG_IPGR1_DEF	(10 << 10)
# define E1000_TIPG_IPGR2_DEF	(10 << 20)
#define E1000_REG_RDBAL		0x2800
#define E1000_REG_RDBAH		0x2804
#define E1000_REG_RDLEN		0x2808
#define E1000_REG_RDH		0x2810
#define E1000_REG_RDT		0x2818
#define E1000_REG_RXDCTL	0x2828
# define E1000_RXDCTL_ENABLE	(1 << 25)
#define E1000_REG_TDBAL		0x3800
#define E1000_REG_TDBAH		0x3804
#define E1000_REG_TDLEN		0x3808
#define E1000_REG_TDH		0x3810
#define E1000_REG_TDT		0x3818
#define E1000_REG_TXDCTL	0x3828
# define E1000_TXDCTL_ENABLE	(1 << 25)
#define E1000_REG_RAL		0x5400
#define E1000_REG_RAH		0x5404
# define E1000_RAH_AV		(1 << 31)

#define E1000_PHY_CTRL		0
# define E1000_PHYC_POWER_DOWN	(1 << 11)

struct eth_header {
	u8	dst[6];
	u8	src[6];
	u16	type;
	u8	data[];
} __attribute__((packed));

#define FRAME_TYPE_ANNOUNCE	0x004a
#define FRAME_TYPE_TARGET_ROLE	0x014a
#define FRAME_TYPE_PING		0x024a
#define FRAME_TYPE_PONG		0x034a

struct e1000_rxd {
	u64	addr;
	u16	len;
	u16	crc;
	u8	dd:1,
		eop:1,
		ixsm:1,
		vp:1,
		udpcs:1,
		tcpcs:1,
		ipcs:1,
		pif:1;
	u8	errors;
	u16	vlan_tag;
} __attribute__((packed));

struct e1000_txd {
	u64	addr;
	u16	len;
	u8	cso;
	u8	eop:1,
		ifcs:1,
		ic:1,
		rs:1,
		rps:1,
		dext:1,
		vle:1,
		ide:1;
	u8	dd:1,
		ec:1,
		lc:1,
		tu:1,
		rsv:4;
	u8	css;
	u16	special;
} __attribute__((packed));

#define RX_DESCRIPTORS		8
#define RX_BUFFER_SIZE		2048
#define TX_DESCRIPTORS		8

static const char *speed_info[] = { "10", "100", "1000", "1000" };

static void *mmiobar;
static u8 buffer[RX_DESCRIPTORS * RX_BUFFER_SIZE];
static struct e1000_rxd rx_ring[RX_DESCRIPTORS] __attribute__((aligned(128)));
static struct e1000_txd tx_ring[TX_DESCRIPTORS] __attribute__((aligned(128)));
static unsigned int rx_idx, tx_idx;
static struct eth_header tx_packet;

static u16 phy_read(unsigned int reg)
{
	u32 val;

	mmio_write32(mmiobar + E1000_REG_MDIC,
		     (reg << E1000_MDIC_REGADD_SHFT) |
		     E1000_MDIC_PHYADD | E1000_MDIC_OP_READ);
	do {
		val = mmio_read32(mmiobar + E1000_REG_MDIC);
		cpu_relax();
	} while (!(val & E1000_MDIC_READY));

	return (u16)val;
}

static void phy_write(unsigned int reg, u16 val)
{
	mmio_write32(mmiobar + E1000_REG_MDIC,
		     val | (reg << E1000_MDIC_REGADD_SHFT) |
		     E1000_MDIC_PHYADD | E1000_MDIC_OP_WRITE);
	while (!(mmio_read32(mmiobar + E1000_REG_MDIC) & E1000_MDIC_READY))
		cpu_relax();
}

static void send_packet(void *buffer, unsigned int size)
{
	unsigned int idx = tx_idx;

	memset(&tx_ring[idx], 0, sizeof(struct e1000_txd));
	tx_ring[idx].addr = (unsigned long)buffer;
	tx_ring[idx].len = size;
	tx_ring[idx].rs = 1;
	tx_ring[idx].ifcs = 1;
	tx_ring[idx].eop = 1;

	tx_idx = (tx_idx + 1) % TX_DESCRIPTORS;
	mmio_write32(mmiobar + E1000_REG_TDT, tx_idx);

	while (!tx_ring[idx].dd)
		cpu_relax();
}

static struct eth_header *packet_received(void)
{
	if (rx_ring[rx_idx].dd)
		return (struct eth_header *)rx_ring[rx_idx].addr;

	cpu_relax();
	return NULL;
}

static void packet_reception_done(void)
{
	unsigned int idx = rx_idx;

	rx_ring[idx].dd = 0;
	rx_idx = (rx_idx + 1) % RX_DESCRIPTORS;
	mmio_write32(mmiobar + E1000_REG_RDT, idx);
}

void inmate_main(void)
{
	enum { ROLE_UNDEFINED, ROLE_CONTROLLER, ROLE_TARGET } role;
	unsigned long min = -1, max = 0, rtt;
	struct eth_header *rx_packet;
	unsigned long long start;
	bool first_round = true;
	unsigned int n;
	u32 eerd, val;
	u8 mac[6];
	u64 bar;
	int bdf;

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
	mmiobar = (void *)(bar & ~0xfUL);
	map_range(mmiobar, 128 * 1024, MAP_UNCACHED);
	printk("MMIO register BAR at %p\n", mmiobar);

	pci_write_config(bdf, PCI_CFG_COMMAND,
			 PCI_CMD_MEM | PCI_CMD_MASTER, 2);

	mmio_write32(mmiobar + E1000_REG_CTRL, E1000_CTRL_RST);
	delay_us(20000);

	val = mmio_read32(mmiobar + E1000_REG_CTRL);
	val &= ~(E1000_CTRL_LRST | E1000_CTRL_FRCSPD);
	val |= E1000_CTRL_SLU;
	mmio_write32(mmiobar + E1000_REG_CTRL, val);

	/* power up again in case the previous user turned it off */
	phy_write(E1000_PHY_CTRL,
		  phy_read(E1000_PHY_CTRL) & ~E1000_PHYC_POWER_DOWN);

	printk("Waiting for link...");
	while (!(mmio_read32(mmiobar + E1000_REG_STATUS) & E1000_STATUS_LU))
		cpu_relax();
	printk(" ok\n");

	val = mmio_read32(mmiobar + E1000_REG_STATUS) & E1000_STATUS_SPEED;
	val >>= E1000_STATUS_SPEEDSHFT;
	printk("Link speed: %s Mb/s\n", speed_info[val]);

	if (mmio_read32(mmiobar + E1000_REG_RAH) & E1000_RAH_AV) {
		*(u32 *)mac = mmio_read32(mmiobar + E1000_REG_RAL);
		*(u16 *)&mac[4] = mmio_read32(mmiobar + E1000_REG_RAH);
	} else {
		for (n = 0; n < 3; n++) {
			mmio_write32(mmiobar + E1000_REG_EERD,
				     E1000_EERD_START |
				     (n << E1000_EERD_ADDR_SHIFT));
			do {
				eerd = mmio_read32(mmiobar + E1000_REG_EERD);
				cpu_relax();
			} while (!(eerd & E1000_EERD_DONE));
			mac[n * 2] = (u8)(eerd >> E1000_EERD_DATA_SHIFT);
			mac[n * 2 + 1] =
				(u8)(eerd >> (E1000_EERD_DATA_SHIFT + 8));
		}
	}

	printk("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	mmio_write32(mmiobar + E1000_REG_RAL, *(u32 *)mac);
	mmio_write32(mmiobar + E1000_REG_RAH, *(u16 *)&mac[4] | E1000_RAH_AV);

	for (n = 0; n < RX_DESCRIPTORS; n++)
		rx_ring[n].addr = (unsigned long)&buffer[n * RX_BUFFER_SIZE];
	mmio_write32(mmiobar + E1000_REG_RDBAL, (unsigned long)&rx_ring);
	mmio_write32(mmiobar + E1000_REG_RDBAH, 0);
	mmio_write32(mmiobar + E1000_REG_RDLEN, sizeof(rx_ring));
	mmio_write32(mmiobar + E1000_REG_RDH, 0);
	mmio_write32(mmiobar + E1000_REG_RDT, 0);
	mmio_write32(mmiobar + E1000_REG_RXDCTL,
		mmio_read32(mmiobar + E1000_REG_RXDCTL) | E1000_RXDCTL_ENABLE);

	val = mmio_read32(mmiobar + E1000_REG_RCTL);
	val |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_BSIZE_2048 |
		E1000_RCTL_SECRC;
	mmio_write32(mmiobar + E1000_REG_RCTL, val);

	mmio_write32(mmiobar + E1000_REG_RDT, RX_DESCRIPTORS - 1);

	mmio_write32(mmiobar + E1000_REG_TDBAL, (unsigned long)&tx_ring);
	mmio_write32(mmiobar + E1000_REG_TDBAH, 0);
	mmio_write32(mmiobar + E1000_REG_TDLEN, sizeof(tx_ring));
	mmio_write32(mmiobar + E1000_REG_TDH, 0);
	mmio_write32(mmiobar + E1000_REG_TDT, 0);
	mmio_write32(mmiobar + E1000_REG_TXDCTL,
		mmio_read32(mmiobar + E1000_REG_TXDCTL) | E1000_TXDCTL_ENABLE);

	val = mmio_read32(mmiobar + E1000_REG_TCTL);
	val |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT_DEF |
		E1000_TCTL_COLD_DEF;
	mmio_write32(mmiobar + E1000_REG_TCTL, val);
	mmio_write32(mmiobar + E1000_REG_TIPG,
		     E1000_TIPG_IPGT_DEF | E1000_TIPG_IPGR1_DEF |
		     E1000_TIPG_IPGR2_DEF);

	role = ROLE_UNDEFINED;

	memcpy(tx_packet.src, mac, sizeof(tx_packet.src));
	memset(tx_packet.dst, 0xff, sizeof(tx_packet.dst));
	tx_packet.type = FRAME_TYPE_ANNOUNCE;
	send_packet(&tx_packet, sizeof(tx_packet));

	start = pm_timer_read();
	while (pm_timer_read() - start < NS_PER_MSEC &&
	       role == ROLE_UNDEFINED) {
		rx_packet = packet_received();
		if (!rx_packet)
			continue;

		if (rx_packet->type == FRAME_TYPE_TARGET_ROLE) {
			role = ROLE_TARGET;
			memcpy(tx_packet.dst, rx_packet->src,
			       sizeof(tx_packet.dst));
		}
		packet_reception_done();
	}

	if (role == ROLE_UNDEFINED) {
		role = ROLE_CONTROLLER;
		printk("Waiting for peer\n");
		while (1) {
			rx_packet = packet_received();
			if (!rx_packet)
				continue;

			if (rx_packet->type == FRAME_TYPE_ANNOUNCE) {
				memcpy(tx_packet.dst, rx_packet->src,
				       sizeof(tx_packet.dst));
				packet_reception_done();

				tx_packet.type = FRAME_TYPE_TARGET_ROLE;
				send_packet(&tx_packet, sizeof(tx_packet));
				break;
			} else {
				packet_reception_done();
			}
		}
	}

	mmio_write32(mmiobar + E1000_REG_RCTL,
		     mmio_read32(mmiobar + E1000_REG_RCTL) & ~E1000_RCTL_BAM);

	if (role == ROLE_CONTROLLER) {
		printk("Running as controller\n");
		tx_packet.type = FRAME_TYPE_PING;
		while (1) {
			start = pm_timer_read();
			send_packet(&tx_packet, sizeof(tx_packet));

			do
				rx_packet = packet_received();
			while (!rx_packet ||
			       rx_packet->type != FRAME_TYPE_PONG);
			packet_reception_done();

			if (!first_round) {
				rtt = pm_timer_read() - start;
				if (rtt < min)
					min = rtt;
				if (rtt > max)
					max = rtt;
				printk("Received pong, RTT: %6ld ns, "
				       "min: %6ld ns, max: %6ld ns\n",
				       rtt, min, max);
			}
			first_round = false;
			delay_us(100000);
		}
	} else {
		printk("Running as target\n");
		tx_packet.type = FRAME_TYPE_PONG;
		while (1) {
			rx_packet = packet_received();
			if (!rx_packet || rx_packet->type != FRAME_TYPE_PING)
				continue;
			packet_reception_done();
			send_packet(&tx_packet, sizeof(tx_packet));
		}
	}
}
