#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

include $(CONFIG_MK)

KBUILD_AFLAGS := $(filter-out -include asm/unified.h,$(KBUILD_AFLAGS))

always := built-in.o

obj-y := entry.o dbg-write.o exception.o setup.o control.o lib.o
obj-y += traps.o mmio.o
obj-y += paging.o mmu_hyp.o mmu_cell.o caches.o
obj-y += psci.o psci_low.o spin.o
obj-y += irqchip.o gic-common.o
obj-$(CONFIG_ARM_GIC_V3) += gic-v3.o
obj-$(CONFIG_SERIAL_AMBA_PL011) += dbg-write-pl011.o
