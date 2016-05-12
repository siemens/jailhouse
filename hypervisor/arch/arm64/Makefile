#
# Jailhouse AArch64 support
#
# Copyright (C) 2015 Huawei Technologies Duesseldorf GmbH
#
# Authors:
#  Antonios Motakis <antonios.motakis@huawei.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

include $(CONFIG_MK)

KBUILD_AFLAGS := $(subst -include asm/unified.h,,$(KBUILD_AFLAGS))

COMMON := ../arm-common

always := built-in.o

obj-y := entry.o setup.o control.o mmio.o caches.o
obj-y += $(COMMON)/dbg-write.o $(COMMON)/lib.o
obj-y += $(COMMON)/mmu_cell.o $(COMMON)/paging.o

obj-$(CONFIG_SERIAL_AMBA_PL011) += $(COMMON)/dbg-write-pl011.o
