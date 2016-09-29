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

KBUILD_AFLAGS := $(subst -include asm/unified.h,,$(KBUILD_AFLAGS))

always := built-in.o

COMMON := ../arm-common

obj-y := entry.o exception.o setup.o control.o traps.o mmio.o
obj-y += mmu_hyp.o caches.o smp.o
obj-y += $(COMMON)/dbg-write.o $(COMMON)/lib.o $(COMMON)/psci.o
obj-y += $(COMMON)/paging.o $(COMMON)/mmu_cell.o
obj-y += $(COMMON)/irqchip.o $(COMMON)/gic-common.o
obj-$(CONFIG_ARM_GIC_V2) += $(COMMON)/gic-v2.o
obj-$(CONFIG_ARM_GIC_V3) += gic-v3.o
obj-$(CONFIG_SERIAL_AMBA_PL011) += $(COMMON)/dbg-write-pl011.o
obj-$(CONFIG_SERIAL_8250_DW) += uart-8250-dw.o
obj-$(CONFIG_SERIAL_TEGRA) += uart-tegra.o
obj-$(CONFIG_MACH_VEXPRESS) += smp-vexpress.o
